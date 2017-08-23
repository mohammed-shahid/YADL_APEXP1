# YADL_APEXP1
YAMCS mutual-exclusion requires the apex-lock winning process to link-in at the end of the MCS queue that it forms.

APEXP1 variant solves that problem and thus reduces high max-delay figures.

Comparison of the max-delay figures for YAMCS_APEXP1 Vs YAMCS is in graphs that are uploaded. Also note that YAMCS (as also YAMCS_APEXP1) are poor in terms of throughput performance, while better in terms of Remote Accesses (per C-S entry) performance.

Commands to be used on the cluster are as in the YAL and YADL laboratories.

YAL also contains instructions to set up a starcluster of desired size on AWS, which is required initially.

Necessary bash commands are given here for copy-paste ease, with N=32:

/usr/bin/mpicc -lpthread YADL_ApexP1_*.c -o YADL_ApexP1 -lm

for i in {20..40..2}; do /usr/bin/mpirun -host master,node001,node002,node003,node004,node005,node006,node007,node008,node009,node010,node011,node012,node013,node014,node015,node016,node017,node018,node019,node020,node021,node022,node023,node024,node025,node026,node027,node028,node029,node030,node031 ./YADL_ApexP1 $i; done
