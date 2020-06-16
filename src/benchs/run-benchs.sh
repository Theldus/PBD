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
if [ ! -x "$(command -v gdb)" ]
then
	echo "GDB not found! GDB and Rscript is necessary in order to"
	echo "run the benchmarks."
	exit 1
fi

if [ ! -x "$(command -v Rscript)" ]
then
	echo "Rscript not found! GDB and Rscript is necessary in order to"
	echo "run the benchmarks."
	exit 1
fi

vars=$(../pbd ./bench do_work1 -d | grep "Variable found" | cut -d':' -f2 | tr -d ' ')
IFS=$'
'
unset IFS
vars=($vars)

# Create the GDB script file
{
	echo "set can-use-hw-watchpoints 0"
	echo "set confirm off"
} > gdb_script

for var in "${vars[@]}"
do
	{
		echo "watch -l $var"
		echo "commands"
		echo "c"
		echo "end"
	} >> gdb_script
done
echo "r" >> gdb_script

# Setup environment
increment=12500
size=$increment

# ------------------------
# PBD work
# Arguments:
# $1 = work number
# $2 = size
# $3 = optional arguments
# ------------------------
function pbd_work()
{
start_time="$(date -u +%s.%N)"
	../pbd ./bench do_work"$1" "$size" "$1" "$3" &>/dev/null
end_time="$(date -u +%s.%N)"
	elapsed_pbd="$(bc <<<"$end_time-$start_time" | awk '{printf "%f", $0}')"
	echo "$elapsed_pbd"
}

# --------------------
# GDB work
# Arguments
# $1 = work number
# $2 = size
# --------------------
function gdb_work()
{
start_time="$(date -u +%s.%N)"
	gdb -batch -x gdb_script --args ./bench "$size" "$1" &>/dev/null
end_time="$(date -u +%s.%N)"
	elapsed_gdb="$(bc <<<"$end_time-$start_time" | awk '{printf "%f", $0}')"
	echo "$elapsed_gdb"
}

# Prepare CSV
echo "size, normal_pbd, static_pbd, gdb" > csv_times
echo -e "\nBeware, this may take up to 3 hours to finish..."

for (( work=1; work<4; work++ ))
do
	echo "> do_work$work..."

	for (( step=1; step<9; step++ ))
	do
		size=$((step*12500))

		# Normal PBD
		echo "    > PBD default...."
		time_pbd="$(pbd_work "$work" $size "")"

		# With static analysis PBD
		echo "    > PBD static...."
		time_static="$(pbd_work "$work" $size "-S")"

		# GDB
		echo "    > GDB...."
		time_gdb="$(gdb_work "$work" $size)"

		echo "    Times: (size=$size), PBD: $time_pbd / PBD Static Analysis: $time_static / GDB: $time_gdb"
		echo "$size, $time_pbd, $time_static, $time_gdb" >> csv_times
		echo ""
	done
done

# Plot graph =)
echo "Plotting graph..."
Rscript plot.R &> /dev/null
