require "mkmf"

dir_config("mpi")

CONFIG['CC'] = "mpicc"

$objs = %w(mpipe.o)

create_makefile("mpipe")
