#!/usr/bin/perl -w
# simple qstat xml output parser, prints values specified as command
# line arguments
#
# Author: Ludwig Nussel
#
# Usage Examples:
#
# Print server name:
#   "qstat/server/name"
#
# Print second server rule (numbered from zero):
#   "qstat/server/rules/rule/1"
#
# Print the rule with name "gamename":
#   "qstat/server/rules/rule/[name=gamename]"
#
# Print name of sixth player:
#   "qstat/server/players/player/5/name"
#
# Print clan of player with name "suCk3r":
#   "qstat/server/players/player/[name=suCk3r]/clan"

use strict;
use XML::Bare;
use Data::Dumper;

sub getvalue {
	my $x = shift;
	my @a = split(/\//, shift);
	for my $n (@a) {
		if ($n =~ /^[[:digit:]]+$/) {
			return undef unless exists $x->[$n];
			$x = $x->[$n];
		} elsif ($n =~ /\[(.*)=(.*)\]/) {
			my ($k, $v) = ($1, $2);
			my $r;
			for my $i (@$x) {
				next unless exists $i->{$k};
				if($i->{$k}->{value} eq $v) {
					$r = $i;
					last;
				}
			}
			return undef unless $r;
			$x = $r;
		} else {
			return undef unless exists $x->{$n};
			$x = $x->{$n};
		}
	}
	return $x->{value};
}

sub printvalue {
	my $val = getvalue(@_);
	$val = "(undefined)" unless defined $val;
	print $val, "\n"
}

my $xml = XML::Bare->new(text => join('', <STDIN>))->parse();

for (@ARGV) {
	printvalue($xml, $_);
}
