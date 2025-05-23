# $Id: mk-2nd.awk,v 1.15 2003/11/01 22:42:50 tom Exp $
##############################################################################
# Copyright (c) 1998-2000,2003 Free Software Foundation, Inc.                #
#                                                                            #
# Permission is hereby granted, free of charge, to any person obtaining a    #
# copy of this software and associated documentation files (the "Software"), #
# to deal in the Software without restriction, including without limitation  #
# the rights to use, copy, modify, merge, publish, distribute, distribute    #
# with modifications, sublicense, and/or sell copies of the Software, and to #
# permit persons to whom the Software is furnished to do so, subject to the  #
# following conditions:                                                      #
#                                                                            #
# The above copyright notice and this permission notice shall be included in #
# all copies or substantial portions of the Software.                        #
#                                                                            #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    #
# THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    #
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        #
# DEALINGS IN THE SOFTWARE.                                                  #
#                                                                            #
# Except as contained in this notice, the name(s) of the above copyright     #
# holders shall not be used in advertising or otherwise to promote the sale, #
# use or other dealings in this Software without prior written               #
# authorization.                                                             #
##############################################################################
#
# Author: Thomas E. Dickey
#
# Generate compile-rules for the modules that we are using in libraries or
# programs.  We are listing them explicitly because we have turned off the
# suffix rules (to force compilation with the appropriate flags).  We could use
# make-recursion but that would result in makefiles that are useless for
# development.
#
# Variables:
#	model
#	MODEL (uppercase version of "model"; toupper is not portable)
#	echo (yes iff we will show the $(CC) lines)
#	subset ("none", "base", "base+ext_funcs" or "termlib")
#
# Fields in src/modules:
#	$1 = module name
#	$2 = progs|lib|c++
#	$3 = source-directory
#
# Fields in src/modules past $3 are dependencies
#
BEGIN	{
		found = 0
		using = 0
	}
	/^@/ {
		using = 0
		if (subset == "none") {
			using = 1
		} else if (index(subset,$2) > 0) {
			if (using == 0) {
				if (found == 0) {
					print  ""
					print  "# generated by mk-2nd.awk"
					print  ""
				}
				using = 1
			}
		}
	}
	/^[@#]/ {
		next
	}
	$1 ~ /trace/ {
		if (traces != "all" && traces != MODEL && $1 != "lib_trace")
			next
	}
	{
		if ($0 != "" \
		 && using != 0) {
			found = 1
			if ( $1 != "" ) {
				print  ""
				if ( $2 == "c++" ) {
					compile="CXX"
					suffix=".cc"
				} else {
					compile="CC"
					suffix=".c"
				}
				printf "../%s/%s$o :\t%s/%s%s", model, $1, $3, $1, suffix
				for (n = 4; n <= NF; n++) printf " \\\n\t\t\t%s", $n
				print  ""
				if ( echo == "yes" )
					atsign=""
				else {
					atsign="@"
					printf "\t@echo 'compiling %s (%s)'\n", $1, model
				}
				if ( $3 == "." || srcdir == "." ) {
					dir = $3 "/"
					sub("^\\$\\(srcdir\\)/","",dir);
					sub("^\\./","",dir);
					printf "\t%scd ../%s; $(LIBTOOL_COMPILE) $(%s) $(CFLAGS_%s) -c ../%s/%s%s%s", atsign, model, compile, MODEL, name, dir, $1, suffix
				} else
					printf "\t%scd ../%s; $(LIBTOOL_COMPILE) $(%s) $(CFLAGS_%s) -c %s/%s%s", atsign, model, compile, MODEL, $3, $1, suffix
			} else {
				printf "%s", $1
				for (n = 2; n <= NF; n++) printf " %s", $n
			}
			print  ""
		}
	}
END	{
		print  ""
	}
