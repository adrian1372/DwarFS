#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Creates a fileset with $nfiles empty files, then proceeds to open each one
# and then close it.
#
set $dir=/mnt/dwarfs
set $nfiles=1
set $meandirwidth=1
set $nthreads=1

define fileset name=bigfileset,path=$dir,size=5368709120,entries=$nfiles,dirwidth=$meandirwidth,prealloc

define process name=fileopen,instances=1
{
  thread name=fileopener,memsize=1m,instances=$nthreads
  {
    flowop openfile name=open1,filesetname=bigfileset,fd=1
    flowop statfile name=stat1,filesetname=bigfileset,fd=1
    flowop fsync name=sync1,fd=1
    flowop closefile name=close1,fd=1
  }
}

echo  "Openfiles Version 1.0 personality successfully loaded"
run 1200
