#!/usr/bin/perl

use strict;
use warnings;

use Encode;

# Jyske Bank exports .csv files in a _horrible_ format. What were they
# thinking of? The whole point of being able to export a .csv is to
# import the data into some other application. Why, then, make each
# and every possible non-standard choice:
#
# - Not utf-8 encoded. Really, in 2018?
#
# - Uses semicolon as separator.
#
# - Numbers use decimal comma and period for thousand separators. OK,
#   we do have to cope with the possibility of the use of decimal
#   comma, but what on earth is the purpose of including thousand
#   separators in numbers that are not meant for human consumption?
#   Had they not been present, it would have been a lot more likely
#   that the tool doing the import could detect the decimal comma
#   versus decimal dot.
#
# - Dates are in dd.mm.yyyy format. OK, not as braindead as the US
#   mm/dd/yyyy format, but again, this is data meant to be imported to
#   some other application. Heard of ISO, anyone? yyyy-mm-dd, thank
#   you very much.
#
# Fix it.

while (<>) {
    chomp;
    $_ = encode('utf-8', decode('iso-8859-1', $_));
    my @f = split /(?<=");(?=")/, $_;
    if ($. == 1) {
	print join("\t", @f), "\n";
	next;
    }

    $f[0] =~ m/"([0-9]{2})\.([0-9]{2})\.([0-9]{4})"/
	or die "Expected dd.mm.yyyy date";
    $f[0] = "$3-$2-$1";

    for my $i (4, 5) {
	$f[$i] =~ m/"-?[0-9]{1,3}(\.[0-9]{3})*,[0-9]{2}"/
	    or die "expected number in column $i, found $f[$i]";
	$f[$i] =~ s/[".]//g;
	$f[$i] =~ s/,/./g;
    }
    print join("\t", @f), "\n";
}
