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

vars=$(../pbd ./bench do_work -d | grep "Variable found" | cut -d':' -f2 | tr -d ' ')
IFS=$'
'
unset IFS
vars=($vars)

# Create the GDB script file
{
	echo "set can-use-hw-watchpoints 0"
	echo "set confirm off"
	echo "b do_work"
	echo "r"
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
echo "c" >> gdb_script

# Setup environment
increment=12500
size=$increment

# Prepare CSV
echo "size, time_pbd, time_gdb" > csv_times

echo -e "\nThis may take some minutes, please be patient..."
for (( step=0; step<8; step++ ))
do
	echo    "Test #$step, size: $size"
	echo -n "  > PBD..."

	# Run PBD
start_time="$(date -u +%s.%N)"
	../pbd ./bench do_work "$size" &>/dev/null
end_time="$(date -u +%s.%N)"

	elapsed_pbd="$(bc <<<"$end_time-$start_time" | awk '{printf "%f", $0}')"
	echo -en " $elapsed_pbd secs\n"

	# Run GDB
	echo -n "  > GDB..."
start_time="$(date -u +%s.%N)"
	gdb -batch -x gdb_script --args ./bench "$size" &>/dev/null
end_time="$(date -u +%s.%N)"

	elapsed_gdb="$(bc <<<"$end_time-$start_time" | awk '{printf "%f", $0}')"
	echo -en " $elapsed_gdb secs\n\n"

	echo "$size, $elapsed_pbd, $elapsed_gdb" >> csv_times
	size=$((size + increment))
done

# Plot graph =)
echo "Plotting graph..."
Rscript plot.R &> /dev/null
