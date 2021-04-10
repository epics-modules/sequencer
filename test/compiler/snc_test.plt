# Do not run this inside the source directory!
# Instead, run snc_test.t inside O.$(EPICS_HOST_ARCH)

use strict;
use Test::More;

my $tests = {
  cast                    => { warnings => 0, errors => 0  },
  change                  => { warnings => 0, errors => 2  },
  delay_in_action         => { warnings => 0, errors => 1  },
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
  pvArray                 => { warnings => 0, errors => 21 },
  pvNotAssigned           => { warnings => 0, errors => 20 },
  reservedId              => { warnings => 0, errors => 2  },
  state_not_reachable     => { warnings => 3, errors => 0  },
  sync_not_assigned       => { warnings => 0, errors => 1  },
  syncq_no_size           => { warnings => 1, errors => 0  },
  syncq_not_assigned      => { warnings => 0, errors => 1  },
  syncq_size_out_of_range => { warnings => 0, errors => 1  },
  type_not_allowed        => { warnings => 2, errors => 9  },
};

my @progs = sort(keys(%$tests));

plan tests => 5 * (@progs + 0);

sub snc_diag {
  diag "snc said this:";
  diag explain $_[0];
}

my $host_arch = $ENV{EPICS_HOST_ARCH};
my $dirsep = '/';
if ("$host_arch" =~ /win32/ || "$host_arch" =~ /windows/) {
  $dirsep = '\\';
}

foreach my $prog (@progs) {
  # prepare source by passing it through CPP
  `make -s -B $prog.i`;
  my $failed = 0;
  # execute the snc and capture the output
  my $output = `..${dirsep}..${dirsep}..${dirsep}bin${dirsep}${host_arch}${dirsep}snc $prog.i -o $prog.c 2>&1`;
  # test whether it terminated normally
  my $exitsig = $? & 127;
  is ($exitsig, 0, "$prog: snc terminates normally") or $failed = 1;
  SKIP: {
    # skip all other tests if snc crashed
    skip("snc died with signal $exitsig", 4) if $exitsig;
    my $exitcode = $? >> 8;
    my $errors_are_expected = $tests->{$prog}->{errors} > 0;
    ok(($exitcode != 0) == $errors_are_expected, "$prog: correct exitcode");
    ok(-e "$prog.c" == ($exitcode == 0), "$prog: success <=> creates output file");
    my $nw = 0;
    $nw++ while ($output =~ /warning/g);
    is($nw, $tests->{$prog}->{warnings}, "$prog: number of warnings") or $failed = 1;
    my $ne = 0;
    $ne++ while ($output =~ /error/g);
    is($ne, $tests->{$prog}->{errors}, "$prog: number of errors") or $failed = 1;
  }
  snc_diag($output) if $failed;
}
