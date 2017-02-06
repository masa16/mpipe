require "mpipe"

MPipe.buffer_size = 2**10

MPipe.init
rank = MPipe::Comm.rank
size = MPipe::Comm.size
puts "size=%d rank=%d pid=%d" % [size, rank, Process.pid]

p MPipe.new(0).object_id == MPipe.new(0).object_id

if rank == 0

  puts "buffer_size = %d" % MPipe.buffer_size

  len = "Hello from 0".size
  (1..size-1).each do |i|
    p MPipe.new(i).read(len)
  end

else

  sleep rank
  message = "Hello from #{rank}"
  MPipe.new(0).write(message)
  puts "message sent from #{rank}"

end
