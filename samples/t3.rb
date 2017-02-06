require "mpipe"

puts "buffer_size = %d" % MPipe.buffer_size
MPipe.buffer_size = 2**13
puts "buffer_size = %d" % MPipe.buffer_size

MPipe.init
rank = MPipe::Comm.rank
size = MPipe::Comm.size
puts "size=%d rank=%d pid=%d" % [size, rank, Process.pid]

# MPipe.buffer_size = 2**13
p MPipe.abort(0)

if rank == 0

  (1..size-1).each do |i|
    p MPipe.new(i).read
  end

else

  sleep rank
  message = "Hello from #{rank}"
  MPipe.new(0).write(message)
  puts "message sent from #{rank}"

end
