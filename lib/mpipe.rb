require "mpipe.so"

class MPipe

  def self.select(*args)
    rd_list = args[0]  # support only input
    mp_list = []
    io_list = []
    result = Array.new(rd_list.size)
    order = {}
    found = false

    rd_list.each_with_index do |rd,i|
      order[rd] = i
      case rd
      when MPipe
        mp_list << rd
        if rd.test_recv
          result[i] = rd
          found = true
        end
      when IO
        io_list << rd
      else
        raise ArgumentError, "elements should be IO or MPipe"
      end
    end
    if !io_list.empty?
      ready, = IO.select(io_list, nil, nil, 0)
      if ready
        ready.each{|io| result[order[io]] = io}
        found = true
      end
    end
    if found
      return [result.compact,[],[]]
    end

    dt = Rational(1,1000)
    max_dt = Rational(256,1000)
    loop do
      if io_list.empty?
        sleep dt
      else
        ready, = IO.select(io_list, nil, nil, dt)
        if ready
          ready.each{|io| result[order[io]] = io}
          found = true
        end
      end
      mp_list.each do |mp|
        if mp.test_recv
          result[order[mp]] = mp
          found = true
        end
      end
      if found
        return [result.compact,[],[]]
      end
      if dt != max_dt
        dt *= 2 if dt < max_dt
        dt = max_dt if dt > max_dt
      end
    end
  end

end
