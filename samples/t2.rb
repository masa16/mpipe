require "mpipe"

MPipe.init

printf "size=%d rank=%d pid=%d\n", MPipe::Comm.size, MPipe::Comm.rank, Process.pid

rank = MPipe::Comm.rank
size = MPipe::Comm.size

if rank == 0

  pipes = (1..size-1).map{|rank| MPipe.new(rank)}

  while !pipes.empty?
    rsel, = MPipe.select(pipes)
    rsel.each do |r|
      p s=r.read_nonblock(12, exception:false)
      if /end/ =~ s
        pipes.delete(r)
      end
    end
  end

else

  mp = MPipe.new(0)
  sleep size-rank
  mp.write("Hello from #{rank}")
  mp.write("end")

end

MPipe.finalize
