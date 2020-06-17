#!/usr/bin/env bash

#
# MIT License
#
# Copyright (c) 2019-2020 Davidson Francis <davidsondfgl@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

# Fancy colors =)
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

# PBD Folder
PBD_FOLDER=$(readlink -f ../)

echo -n "Tests (normal + static analysis)..."

# Run iterative and recursive tests
{
	"$PBD_FOLDER"/pbd test func1     > outputs/test_func1_out    &&\
	"$PBD_FOLDER"/pbd test func1 -S  > outputs/test_func1_out_sa &&\
	"$PBD_FOLDER"/pbd test factorial
} &> /dev/null

if [ $? -eq 0 ]
then
	# Note:
	#
	# If all tests were able to run and finish without errors, its 'almost'
	# sure that the output will be ok too, nevertheless, we need to ensure.
	#
	#
	#
	if cmp -s "outputs/test_func1_expected" "outputs/test_func1_out"
	then
		if ! cmp -s "outputs/test_func1_expected" "outputs/test_func1_out_sa"
		then
			echo -e " [${RED}NOT PASSED${NC}] (static analysis differ from expected output)"
			exit 1
		fi

		echo -e " [${GREEN}PASSED${NC}]"

		if [ -x "$(command -v valgrind)" ]
		then
			echo -n "Valgrind tests..."

			{
				valgrind\
					--leak-check=full\
					--suppressions=pbd.supp\
					--errors-for-leak-kinds=all\
					--error-exitcode=1\
					"$PBD_FOLDER"/pbd test func1 &&\

				valgrind\
					--leak-check=full\
					--suppressions=pbd.supp\
					--errors-for-leak-kinds=all\
					--error-exitcode=1\
					"$PBD_FOLDER"/pbd test func1 -S &&\

				valgrind\
					--leak-check=full\
					--suppressions=pbd.supp\
					--errors-for-leak-kinds=all\
					--error-exitcode=1\
					"$PBD_FOLDER"/pbd test factorial
			} &> /dev/null

			if [ $? -eq 0 ]
			then
				echo -e " [${GREEN}PASSED${NC}]"
			else
				echo -e " [${RED}NOT PASSED${NC}] (memory leaks)"
				exit 1
			fi
		fi
	else
		echo -e " [${RED}NOT PASSED${NC}] (normal analysis differ from expected output)"
		exit 1
	fi
else
	echo -e " [${RED}NOT PASSED${NC}] (execution error)"
fi

# Run static analysis tests
echo -n "Static parsing analysis tests..."

"$PBD_FOLDER"/pbd test static_analysis_func3 -d -S -D STATIC_ANALYSIS |\
	grep "===static=analysis===" > outputs/test_func3_out_sa

if [ $? -eq 0 ]
then
	if cmp -s "outputs/test_func3_expected_sa" "outputs/test_func3_out_sa"
	then
		echo -e " [${GREEN}PASSED${NC}]"
	fi
else
	echo -e " [${RED}NOT PASSED${NC}]"
	exit 1
fi
