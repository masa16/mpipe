require "mpipe"

MPipe.init
rank = MPipe::Comm.rank
size = MPipe::Comm.size
puts "size=%d rank=%d pid=%d" % [size,rank,Process.pid]

def _read(io,n)
  io.read_nonblock(n)
rescue IO::WaitReadable
  MPipe.select([io])
  retry
end

if rank == 0

  pipes = (1..size-1).map{|rank| MPipe.new(rank)}

  pipes.each do |x|
    while !(s = _read(x,12))
      puts "waiting"
    end
    p s
  end

else

  sleep rank
  message = "Hello from #{rank}"
  MPipe.new(0).write(message)

end
