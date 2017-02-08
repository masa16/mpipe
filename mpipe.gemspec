# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)

# get version
open("ext/mpipe/mpipe.c") do |f|
  f.each_line do |l|
    if /MPIPE_VERSION "([\d.]+)"/ =~ l
      VERSION = $1
      break
    end
  end
end

Gem::Specification.new do |spec|
  spec.name          = "mpipe"
  spec.version       = VERSION
  spec.authors       = ["Masahiro TANAKA"]
  spec.email         = ["masa16.tanaka@gmail.com"]
  spec.description   = %q{MPipe - IO.pipe emulation using MPI.}
  spec.summary       = %q{MPipe - IO.pipe emulation using MPI}
  spec.homepage      = "https://github.com/masa16/mpipe"
  spec.license       = "MIT"

  spec.files         = `git ls-files -z`.split("\x0").reject do |f|
    f.match(%r{^(test|spec|features)/})
  end
  spec.bindir        = "bin"
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})
  spec.require_paths = ["lib"]
  spec.extensions    = %w[ext/mpipe/extconf.rb]

  spec.add_development_dependency "bundler", "~> 1.14"
  spec.add_development_dependency "rake", "~> 10.0"
end
