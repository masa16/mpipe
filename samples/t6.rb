require "mpipe"

MPipe.init
rank = MPipe::Comm.rank
size = MPipe::Comm.size
puts "size=%d rank=%d pid=%d" % [size,rank,Process.pid]

if rank == 0

  pipes = (1..size-1).map{|rank| MPipe.new(rank)}

  pipes.each do |r|
    p r.read(6,"")
    p r.read(6)
  end

else

  sleep rank
  message = "Hello from #{rank}"
  MPipe.new(0).write(message)

end
