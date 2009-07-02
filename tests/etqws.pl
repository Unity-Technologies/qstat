#!/usr/bin/perl -w
# simple responder for doom3/quake4/etqw queries

use strict;
use IO::Socket::INET;
use IO::File;

my $sock = IO::Socket::INET->new(
	LocalAddr => 'localhost',
	Proto     => 'udp');

print $sock->sockport(),"\n";

my $rin = '';
vec($rin, $sock->fileno, 1) = 1;

my @files = @ARGV;
die "USAGE: $0 <file>" unless @files;

sub _stat_size($)
{
	my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	      $atime,$mtime,$ctime,$blksize,$blocks) = stat($_[0]);
	return $size;
}

sub readfile($)
{
	my $fn = shift;
	print "reading $fn\n";
	my $f = IO::File->new($fn, "r");
	my $buf;
	$f->read($buf, _stat_size($f));
	$f->close;
	return $buf;
}

while (select(my $rout = $rin, undef, undef, undef)) {
	my $data = '';
	my $hispaddr;
	$hispaddr = $sock->recv($data, 0xffff, 0) || die "recv: $!";
	my ($port, $hisiaddr) = sockaddr_in($hispaddr);
	printf '%d bytes from %s:%d'."\n", length($data), inet_ntoa($hisiaddr), $port;

	if($data !~ /^\xff\xffgetInfo/) {
		printf 'invalid packet from %s:%d'."\n", inet_ntoa($hisiaddr), $port;
		next;
	}

	my $buf = readfile($files[0]);

	$sock->send($buf, 0, $hispaddr);
}
