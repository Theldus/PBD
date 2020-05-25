#!/usr/bin/env bash

#
# MIT License
#
# Copyright (c) 2019 Davidson Francis <davidsondfgl@gmail.com>
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

echo -n "Tests..."

# Run iterative and recursive tests
{
	"$PBD_FOLDER"/pbd test func1     > test_func1_out &&\
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
	if cmp -s "test_func1_expected" "test_func1_out"
	then
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
					"$PBD_FOLDER"/pbd test factorial
			} &> /dev/null

			if [ $? -eq 0 ]
			then
				echo -e " [${GREEN}PASSED${NC}]"
			else
				echo -e " [${RED}NOT PASSED${NC}]"
				exit 1
			fi
		fi
		exit 0
	else
		echo -e " [${RED}NOT PASSED${NC}]"
		exit 1
	fi
fi
