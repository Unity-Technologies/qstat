This directory contains the output templates used for the
old broccoli server status page.

** The broccoli servers are no longer running.  You should substitute
** your own server addresses into 'broc.lst'.

brocTh.html     header template
brocTp.html     player template
brocTs.html     server template
brocTt.html     trailer template
broc.lst        server list

To generate an HTML page for the broccoli servers:

qstat -P -f broc.lst -Ts brocTs.html -Th brocTh.html -Tt brocTt.html -Tp brocTp.html -sort g -of qservers.html

The HTML output will be put in "qservers.html".



I've also included some templates for Unreal/UT.  They can be used
like this:

qstat -P -R -f unreal.lst -Th unrealTh.html -Tp unrealTp.html -Ts unrealTs.html -Tt unrealTt.html -sort g -of unreal.html

The HTML output will be put in "unreal.html".


qstat -P -R -f tribes2.lst -Th tribes2th.html -Tp tribes2tp.html -Ts tribes2ts.html -Tt tribes2tt.html -sort g -of tribes2.html
