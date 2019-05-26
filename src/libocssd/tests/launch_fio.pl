#! /usr/bin/perl
#
# Copyright (C) 2018-2019 by Beijing Starblaze Technology Co., Ltd.
#
# NOTICE:  All information contained herein is, and remains the
# property of Beijing Starblaze Technology Co., Ltd and its
# suppliers, if any.  The intellectual and technical concepts
# contained herein are proprietary to Beijing Starblaze
# Technology Co., Ltd and are protected by patent or trade
# secret or copyright law.
#
# Dissemination of this information or reproduction of this
# material is strictly forbidden unless prior written permission
# is obtained from Beijing Starblaze Technology Co., Ltd. Source
# code from this file can only be used on Starblaze ASIC products.
#
# Distribution of source code or binaries derived from this file
# is not permitted except as specifically allowed By Non_Disclosure
# Agreement between your company and Beijing Starblaze Technology
# Co., Ltd. All copies of this source code modified or unmodified
# must retain this entire copyright notice and comment as is.
#
# You should have received a copy of the Non-Disclosure Agreement
# along with this program; if not, Please contact your manager
# immediately.
#

use strict;
$| = 1;

my $flag_analyze_iops = 0;
my $flag_analyze_bandwidth = 0;
my $flag_analyze_latency = 0;

# Expand argument
my $cmd_str = "";
for (my $i = 0; $i < scalar(@ARGV); $i++) {
    if ($ARGV[$i] eq "-i") {
        $flag_analyze_iops = 1;
    } elsif ($ARGV[$i] eq "-b") {
        $flag_analyze_bandwidth = 1;
    } elsif ($ARGV[$i] eq "-l") {
        $flag_analyze_latency = 1;
    } else {
        $cmd_str = "${cmd_str} $ARGV[$i]";
    }
}

#printf("$cmd_str\n");

my $res_str = `$cmd_str 2>&1`;
my $flag_fail = 0;
my $flag_keyword_found = 0;

my $log_file = "log_$cmd_str";
$log_file =~ s/\./_/g;
$log_file =~ s/\//_/g;
$log_file =~ s/\\/_/g;
$log_file =~ s/\s/_/g;
$log_file =~ s/\t/_/g;
$log_file = "${log_file}.log";

open OUT, "> $log_file";
my @iops_logs;
my @bandwidth_logs;
my @latency_logs;

foreach my $line (split /\n/, $res_str) {
    chomp($line);
    printf OUT "$line\n";
#    printf("$line\n");
    if ($line =~ /bad/) {
        $flag_fail = 1;
        last;
    } elsif ($line =~ /Error/) {
        $flag_fail = 1;
        last;
    } elsif ($line =~ /magic/) {
        $flag_fail = 1;
        last;
    } elsif ($line =~ /not\sfound/) {
        $flag_fail = 1;
        last;
    } elsif ($line =~ /error=Permission\sdenied/) {
        $flag_fail = 1;
        last;
    } elsif ($line =~ /error=[1-9]/) {
        $flag_fail = 1;
        last;
    } elsif ($line =~ /^\s*WRITE:/) {
        push @bandwidth_logs, $line;
    } elsif ($line =~ /^\s*READ:/) {
        push @bandwidth_logs, $line;
    } elsif ($line =~ /,\siops=/) {
        push @iops_logs, $line;
    } elsif ($line =~ /clat.+,\savg=/) {
        push @latency_logs, $line;
    } 

    if ($line =~ /err=\s0/) {
        $flag_keyword_found = 1;
    }
}

close OUT;

if ($flag_keyword_found == 0) {
    $flag_fail = 1;
}

if ($flag_analyze_bandwidth) {
    foreach my $log (@bandwidth_logs) {
        printf "$log\n";
    }
}


if ($flag_analyze_iops) {
    foreach my $log (@iops_logs) {
        printf "$log\n";
    }
}

if ($flag_analyze_latency) {
    foreach my $log (@latency_logs) {
        printf "$log\n";
    }
}



if ($flag_fail) {
##    printf "FAIL\n";
    exit(-1);
} else {
##    printf "PASS\n";
    exit(0);
}



