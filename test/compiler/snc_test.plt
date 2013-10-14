# Do not run this inside the source directory!
# Instead, run snc_test.t inside O.$(EPICS_HOST_ARCH)

use strict;
use Test::More;

my $tests = {
  cast                    => { warnings => 0, errors => 0  },
  efArray                 => { warnings => 0, errors => 1  },
  efGlobal                => { warnings => 0, errors => 3  },
  efNoInit                => { warnings => 0, errors => 1  },
  efPointer               => { warnings => 0, errors => 1  },
  exOpt_UnrecOpt          => { warnings => 1, errors => 0  },
  foreignGlobal           => { warnings => 1, errors => 3  },
  foreignNoInit           => { warnings => 0, errors => 1  },
  foreignTypes            => { warnings => 1, errors => 0  },
  funcdefShadowGlobal     => { warnings => 0, errors => 1  },
  misplacedExit           => { warnings => 0, errors => 1  },
  namingConflict          => { warnings => 0, errors => 0  },
  nesting_depth           => { warnings => 0, errors => 0  },
  pvNotAssigned           => { warnings => 0, errors => 20 },
  reservedId              => { warnings => 0, errors => 2  },
  state_not_reachable     => { warnings => 3, errors => 0  },
  sync_not_assigned       => { warnings => 0, errors => 1  },
  syncq_no_size           => { warnings => 1, errors => 0  },
  syncq_not_assigned      => { warnings => 0, errors => 1  },
  syncq_size_out_of_range => { warnings => 0, errors => 1  },
};

my @progs = sort(keys(%$tests));

plan tests => @progs + 0;

sub snc_diag {
  diag "snc said this:";
  diag explain $_[0];
}

foreach my $prog (@progs) {
  subtest $prog => sub {
    plan tests => 3;
    my $output = `make -s -B $prog.c 2>&1`;
    my $exitcode = $? >> 8;
    SKIP: {
      skip "errors are expected", 1 if $tests->{$prog}->{errors} > 0;
      is($exitcode, 0, "good exitcode") or snc_diag($output);
    }
    my $nw = 0;
    $nw++ while ($output =~ /warning/g);
    is($nw, $tests->{$prog}->{warnings}, "number of warnings") or snc_diag($output);
      
    my $ne = 0;
    $ne++ while ($output =~ /error/g);
    is($ne, $tests->{$prog}->{errors}, "number of errors") or snc_diag($output);
  }
}
