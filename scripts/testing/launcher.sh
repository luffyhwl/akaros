#!/bin/bash
# This script should be called from Jenkins when a new commit has been pushed 
# to the repo. 
# It analyzes what parts of the codebase have been modified, compiles everything
# that is needed, and reports on the results. 

set -e

readonly TMP_DIR=tmp
readonly DIFF_FILE=$TMP_DIR/changes.txt
readonly AKAROS_OUTPUT_FILE=$TMP_DIR/akaros_out.txt # If changed, please
													# change .py test files too.
readonly TEST_OUTPUT_DIR=output-tests
readonly TEST_DIR=scripts/testing
readonly SCR_DIR=scripts/testing/utils

# Config files
readonly CONF_DIR=scripts/testing/config
readonly CONF_COMP_COMPONENTS_FILE=$CONF_DIR/compilation_components.json

# Utility scripts
readonly SCR_WAIT_UNTIL=$SCR_DIR/wait_until.py
readonly SCR_GIT_CHANGES=$SCR_DIR/changes.py



################################################################################
###############                   INITIAL SETUP                  ###############
################################################################################

if [ "$INITIAL_SETUP" == true ]; then
	# Create directory for tests and other temp files.
	mkdir -p $TMP_DIR
	mkdir -p $TEST_OUTPUT_DIR

	# Compile QEMU launcher
	mkdir -p $WORKSPACE/install/qemu_launcher/
	gcc $SCR_DIR/qemu_launcher.c -o install/qemu_launcher/qemu_launcher

	echo "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *"
	echo "Set up finished succesfully."
	echo "Please run sudo chown root:root install/qemu_launcher/qemu_launcher"
	echo "Please run sudo chmod 4755 install/qemu_launcher/qemu_launcher"
	echo "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *"
	echo ""
	exit 0
fi



################################################################################
###############                 PRE BUILD SETUP                  ###############
################################################################################

function add_cross_compiler_to_path() {
	export PATH=$WORKSPACE/install/riscv-ros-gcc/bin:$PATH
	export PATH=$WORKSPACE/install/i686-ros-gcc/bin:$PATH
	export PATH=$WORKSPACE/install/x86_64-ros-gcc/bin:$PATH
	export PATH=$WORKSPACE/install/x86_64-ros-gcc-native/bin:$PATH
}

# Clean these two directories
rm $TMP_DIR/* $TEST_OUTPUT_DIR/* -f
add_cross_compiler_to_path


################################################################################
###############                    COMPILATION                   ###############
################################################################################

function build_config() {
	make ARCH=x86 defconfig

	# Enable postboot kernel tests to run.
	# These don't take much to execute so we can run them always and just parse
	# results if needed
	echo "CONFIG_POSTBOOT_KERNEL_TESTING=y" >> .config
}

function build_cross_compiler() {
	cd tools/compilers/gcc-glibc

	# Define cross compiler Makelocal.
	echo "# Number of make jobs to spawn.  
MAKE_JOBS := 3
RISCV_INSTDIR         := $WORKSPACE/install/riscv-ros-gcc/
I686_INSTDIR          := $WORKSPACE/install/i686-ros-gcc/
X86_64_INSTDIR        := $WORKSPACE/install/x86_64-ros-gcc/
X86_64_NATIVE_INSTDIR := $WORKSPACE/install/x86_64-ros-gcc-native/
" > Makelocal

	# Directories where the cross compiler will be installed.
	mkdir -p $WORKSPACE/install/riscv-ros-gcc/
	mkdir -p $WORKSPACE/install/i686-ros-gcc/
	mkdir -p $WORKSPACE/install/x86_64-ros-gcc/
	mkdir -p $WORKSPACE/install/x86_64-ros-gcc-native/

	# Compile cross compiler.
	make x86_64

	# Go back to root directory.
	cd ../../..
}

function build_kernel() {
	make
}

function build_userspace() {
	# This is needed because of a bug that won't let tests to be compiled
	# unless the following files are present.
	cd kern/kfs/bin
	touch busybox
	touch chmod
	cd -

	# Compile tests.
	make tests

	# Fill memory with tests.
	make fill-kfs

	# Recompile kernel.
	make
}

function build_busybox() {
	# TO DO
	echo "[TO DO] Build busybox"
}

function run_qemu() {
	echo "-include scripts/testing/Makelocal_qemu" > Makelocal
	export PATH=$WORKSPACE/install/qemu_launcher/:$PATH
	make qemu > $AKAROS_OUTPUT_FILE &
	MAKE_PID=$!

	$SCR_WAIT_UNTIL $AKAROS_OUTPUT_FILE END_PB_KERNEL_TESTS

	# Extract Qemu_launcher PID
	QEMU_PID=$(ps --ppid $MAKE_PID | grep qemu_launcher | \
	           sed -e 's/^\s*//' | cut -d' ' -f1)

	kill -10 $QEMU_PID

	wait $MAKE_PID
}



if [ "$COMPILE_ALL" == true ]; then
	echo "Building all AKAROS"
	build_config
	
	build_cross_compiler
	build_kernel
	build_userspace
	build_busybox

	run_qemu

	# TODO: Fill AFFECTED_COMPONENTS with everything
else
	# Save changed files between last tested commit and current one.
	git diff --stat $GIT_PREVIOUS_COMMIT $GIT_COMMIT > $DIFF_FILE

	# Extract build targets by parsing diff file.
	AFFECTED_COMPONENTS=`$SCR_GIT_CHANGES $DIFF_FILE $CONF_COMP_COMPONENTS_FILE`

	# AFFECTED_COMPONENTS can contain {cross-compiler, kernel, userspace, busybox}

	if [[ -n $AFFECTED_COMPONENTS ]]; 
	then
		echo "Detected changes in "$AFFECTED_COMPONENTS
		build_config

		if [[ $AFFECTED_COMPONENTS == *cross-compiler* ]]
		then
			build_cross_compiler
			build_kernel
			build_userspace
			build_busybox
		else 
			if [[ $AFFECTED_COMPONENTS == *kernel* ]]
			then
				build_kernel
			fi

			if [[ $AFFECTED_COMPONENTS == *userspace* ]]
			then
				build_userspace
			fi

			if [[ $AFFECTED_COMPONENTS == *busybox* ]]
			then
				build_busybox
			fi
		fi
	else
		echo "Skipping build. No changes detected."
	fi

	run_qemu
fi


################################################################################
###############                  TEST REPORTING                  ###############
################################################################################

# Prepend '-a ' to all $AFFECTED_COMPONENTS to pass it as argument to nosetests.
IFS=' ' read -a AFFECTED_COMPONENTS_ARRAY <<< "$AFFECTED_COMPONENTS"
TESTS_TO_RUN=""
for COMPONENT in "${AFFECTED_COMPONENTS_ARRAY[@]}"; 
do
	TESTS_TO_RUN="$TESTS_TO_RUN -a $COMPONENT"
done

nosetests $TEST_DIR/test_wrapper.py --with-xunit /
	--xunit-file=$TEST_OUTPUT_DIR/test_output.xml $TESTS_TO_RUN