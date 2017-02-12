require "mpipe.so"

class MPipe

  @@min_poll = 0.01
  @@max_poll = 0.32

  def self.min_polling_interval=(time)
    @@min_poll = time
  end

  def self.max_polling_interval=(time)
    @@max_poll = time
  end

  # emulate IO.select
  def self.select(rd_ary, wt_ary=nil, er_ary=nil, timeout=nil)
    rd_ary = [] if rd_ary.nil?
    wt_ary = [] if wt_ary.nil?
    rd_mpi = []
    rd_io = []
    wt_io = []
    rd_ret = Array.new(rd_ary.size)
    wt_ret = Array.new(wt_ary.size)
    rd_idx = {}
    wt_idx = {}
    found = false

    rd_ary.each_with_index do |rd,i|
      rd_idx[rd] = i
      case rd
      when MPipe
        rd_mpi << rd
        if rd.test_recv
          rd_ret[i] = rd
          found = true
        end
      when IO
        rd_io << rd
      else
        raise ArgumentError, "elements should be IO or MPipe"
      end
    end

    wt_ary.each_with_index do |wt,i|
      wt_idx[wt] = i
      case wt
      when MPipe
        wt_ret[i] = wt
        found = true
      when IO
        wt_io << wt
      else
        raise ArgumentError, "elements should be IO or MPipe"
      end
    end

    if er_ary
      er_ary.each do |er|
        if !er.kind_of?(IO)
          raise ArgumentError, "er_ary contains non-IO object"
        end
      end
    end

    time_start = Time.now

    # first check
    rd_res,wt_res,er_res = IO.select(rd_io, wt_io, er_ary, 0)
    if rd_res
      rd_res.each{|io| rd_ret[rd_idx[io]] = io}
      found = true
    end
    if wt_res
      wt_res.each{|io| wt_ret[wt_idx[io]] = io}
      found = true
    end
    if er_res
      found = true
    end
    if found
      return [rd_ret.compact, wt_ret.compact, er_res]
    end

    dt = @@min_poll
    max_dt = @@max_poll
    loop do
      if timeout
        elap = Time.now - time_start
        if timeout <= elap
          return nil
        else
          dto = timeout - elap
          dt = (dto < dt) ? dto : dt
        end
      end

      # check with timeout
      rd_res,wt_res,er_res = IO.select(rd_io, wt_io, er_ary, dt)

      if rd_res
        rd_res.each{|io| rd_ret[rd_idx[io]] = io}
        found = true
      end
      if wt_res
        wt_res.each{|io| wt_ret[wt_idx[io]] = io}
        found = true
      end
      if er_res
        found = true
      end
      rd_mpi.each do |mp|
        if mp.test_recv
          rd_ret[rd_idx[mp]] = mp
          found = true
        end
      end
      if found
        return [rd_ret.compact,wt_ret.compact,er_res]
      end
      if dt != max_dt
        dt *= 2 if dt < max_dt
        dt = max_dt if dt > max_dt
      end
    end
  end

end
