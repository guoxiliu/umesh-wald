./umeshImportLanderFun3D\
    -o  /mnt/raid/wald/models/umesh/lander-small-vmag-9000.umesh\
    /mnt/raid/wald/models/unstructured/lander-small/geometry/dAgpu0145_Fa_me\
    --scalars /mnt/raid/wald/models/unstructured/lander-small/10000unsteadyiters/dAgpu0145_Fa_volume_data.\
    -var vort_mag -ts 9000\
    -surf /mnt/raid/wald/models/unstructured/a-143m/dAgpu0145_Fa.lb8.ugrid64
cp /mnt/raid/wald/models/umesh/lander-small-vmag-9000.umesh /mnt/raid/wald/nfs/upload/

./umeshDumpSurfaceMesh /mnt/raid/wald/models/umesh/lander-small-vmag-9000.umesh -o /mnt/raid/wald/models/umesh/lander-small-surface.obj
cp /mnt/raid/wald/models/umesh/lander-small-surface.obj /mnt/raid/wald/nfs/upload/




./umeshImportLanderFun3D\
    -o  /mnt/raid/wald/models/umesh/lander-huge-vmag-900.umesh\
    /mnt/raid/wald/models/unstructured/lander-huge/geometry/114B_me\
    --scalars /mnt/raid/wald/models/unstructured/lander-huge/1000unsteadyiters/114B_volume_data.\
    -var vort_mag -ts 900

#    -surf /mnt/raid/wald/models/unstructured/a-143m/dAgpu0145_Fa.lb8.ugrid64

cp /mnt/raid/wald/models/umesh/lander-huge-vmag-900.umesh /mnt/raid/wald/nfs/upload/
./umeshDumpSurfaceMesh /mnt/raid/wald/models/umesh/lander-huge-vmag-9000.umesh\
		       -o /mnt/raid/wald/models/umesh/lander-huge-surface.obj
cp /mnt/raid/wald/models/umesh/lander-huge-surface.obj /mnt/raid/wald/nfs/upload/

