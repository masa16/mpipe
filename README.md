# MPipe

Ruby's IO.pipe emulation over MPI

[GitHub](https://github.com/masa16/mpipe) | [RubyGems](https://rubygems.org/gems/mpipe)

## Requirement

Message Passing Interface (MPI) framework such as 
[OpenMPI](https://www.open-mpi.org/), [MPICH](https://www.mpich.org/), etc.

Required commands:
* mpicc - Compiler for MPI programs.
* mpirun - Command to run MPI program.

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'mpipe'
```

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install mpipe

## Usage

test code: t.rb
```ruby
require "mpipe"

MPipe.init
rank = MPipe::Comm.rank
size = MPipe::Comm.size
puts "size=%d rank=%d pid=%d" % [size,rank,Process.pid]

if rank == 0

  (1..size-1).each do |r|
    p MPipe.new(r).read(12)
  end

else

  sleep rank
  MPipe.new(0).write("Hello from #{rank}")

end
```

execute:
```
$ mpirun -np 4 ruby t.rb
size=4 rank=0 pid=10353
"Hello from 1"
size=4 rank=1 pid=10354
"Hello from 2"
size=4 rank=2 pid=10355
"Hello from 3"
size=4 rank=3 pid=10356
```

## API

```
MPipe.init(*args) -- calls MPI_Init()
MPipe.finalize -- calls MPI_Finalize()
MPipe.abort(errorcode) -- calls MPI_Abort(MPI_COMM_WORLD, errorcode)
MPipe::Comm.rank -- calls MPI_Comm_rank(), return the rank of this process.
MPipe::Comm.size -- calls MPI_Comm_size(), return the size of this environment.

mp = MPipe.new(rank) -- returns pipe to MPI process with rank.
mp.write(str) -- emulate IO#write.
mp.read(length,outbuf=nil) -- emulate IO#read.
mp.read_nonblock(maxlen,outbuf=nil,excepton:true) -- emulate IO#read_nonblock.
MPipe.select(array_of_mpipe) -- emulate IO.select
```

## Contributing

Bug reports and pull requests are welcome on GitHub at https://github.com/masa16/mpipe.
