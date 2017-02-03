require_relative "../ext/mpipe"

MPipe.init

printf "size=%d rank=%d pid=%d\n", MPipe::Comm.size, MPipe::Comm.rank, Process.pid

rank = MPipe::Comm.rank
size = MPipe::Comm.size

if rank == 0

  pipes = (1..size-1).map{|rank| MPipe.new(rank)}

  while !pipes.empty?
    rsel, = MPipe.select(pipes)
    rsel.each do |r|
      p s=r.read_nonblock
      if /end/ =~ s
        pipes.delete(r)
      end
    end
  end

else

  sleep size-rank
  message = "Hello from #{rank}"
  MPipe.new(0).write(message)
  MPipe.new(0).write("end")

end
