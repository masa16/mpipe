require "mpipe"

MPipe.init
rank = MPipe::Comm.rank
size = MPipe::Comm.size
puts "size=%d rank=%d pid=%d" % [size,rank,Process.pid]

if rank == 0

  pipes = (1..size-1).map{|rank| MPipe.new(rank)}
  p pipes

  while !pipes.empty?
    rsel, = MPipe.select(pipes)
    rsel.each do |r|
      #p r.read_nonblock(6,"")
      p s = r.read_nonblock(74,exception:false)
      pipes.delete(r) if /end/ =~ s
    end
  end

else

  sleep size-rank
  message = "Hello from #{rank} - "*500+"hello end"
  MPipe.new(0).write(message)

end
