data <- read.csv("csv_times", header=TRUE, sep=",")

#PNG Device
png(
	filename="bench.png",
	width = 700, height = 450, 
	units = "px", pointsize = 15
)

# Margins
par(mar = c(5.1, 4.1, 2.5, 2.1))

# Plot PBD
plot(
	data$size, data$time_pbd,
	xlab="Number of iterations", ylab="Time (Seconds)",
	main="Time per iteration (PBD vs GDB)",
	type="l", col="red",
	ylim=c(0,max(data$time_gdb)), lwd=3
)

axis(side=2, at=seq(0, max(data$time_gdb), by=10))

# Plot GDB
lines(data$size, data$time_gdb, col="blue", lwd=3)

#Legend
legend("topright", legend = c("GDB", "PBD"),
	col = c("blue", "red"), lty=c(1,1))

#GRID
grid(NULL, NULL)

#Device off
dev.off()
