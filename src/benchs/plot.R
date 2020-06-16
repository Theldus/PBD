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

# "Constants"
DO_WORK1 <- 1
DO_WORK2 <- 2
DO_WORK3 <- 3
LINE_WIDTH <- 2

data <- read.csv("csv_times", header=TRUE, sep=",")
normal_pbd <- data$normal_pbd
static_pbd <- data$static_pbd
gdb <- data$gdb

# Divide between works
# normal_pbd[[1]] = do_work1
# normal_pbd[[2]] = do_work2
# normal_pbd[[3]] = do_work3
# and so on....
normal_pbd <- split(normal_pbd, ceiling(seq_along(normal_pbd)/8))
static_pbd <- split(static_pbd, ceiling(seq_along(static_pbd)/8))
gdb <- split(gdb, ceiling(seq_along(gdb)/8))

# X axis
x_axis <- seq(from=12500, to=100000, by=12500)

#PNG Device
png(
	filename="bench.png",
	width = 895, height = 341,
	units = "px", pointsize = 15
)

par(oma=c(3,3,3,3),mar=c(3,3,3,1),mfrow=c(1,3))
par(cex.axis=1.2)

# Normal PBD
plot(
	main="PBD default",
	x_axis, normal_pbd[[DO_WORK1]],
	type="l", lty=1, col="red",
	ylim=c(0, max(unlist(normal_pbd))), lwd=LINE_WIDTH
)
lines(x_axis, normal_pbd[[DO_WORK2]], col="blue", lwd=LINE_WIDTH, lty=1)
lines(x_axis, normal_pbd[[DO_WORK3]], col="orange", lwd=LINE_WIDTH, lty=1)
grid(NULL, NULL)
legend("topleft", legend=c("work1", "work2", "work3"),
	col=c("red", "blue", "orange"), pch=15, bty="o", horiz=TRUE, cex=1.15)

# Static
plot(
	main="PBD w/ static analysis",
	x_axis, static_pbd[[DO_WORK1]],
	type="l", lty=1, col="red",
	ylim=c(0, max(unlist(normal_pbd))), lwd=LINE_WIDTH
)
lines(x_axis, static_pbd[[DO_WORK2]], col="blue", lwd=LINE_WIDTH, lty=1)
lines(x_axis, static_pbd[[DO_WORK3]], col="orange", lwd=LINE_WIDTH, lty=1)
grid(NULL, NULL)

# GDB
plot(
	main="GDB",
	x_axis, gdb[[DO_WORK1]],
	type="l", lty=1, col="red",
	ylim=c(0, max(unlist(gdb))), lwd=LINE_WIDTH, pch=0
)
lines(x_axis, gdb[[DO_WORK2]], col="blue", lwd=LINE_WIDTH, lty=1)
lines(x_axis, gdb[[DO_WORK3]], col="orange", lwd=LINE_WIDTH, lty=1)
grid(NULL, NULL)

mtext(text="Time per iteration (PBD vs GDB)",side=3,line=0,outer=TRUE,font=2)
mtext(text="Number of iterations",side=1,line=0,outer=TRUE)
mtext(text="Time (seconds)",side=2,line=0,outer=TRUE)

#Device off
dev.off()
