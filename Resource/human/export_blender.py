import bpy, json, math, struct, hashlib, sys
from pathlib import Path
from mathutils import Matrix
out=Path(sys.argv[sys.argv.index('--')+1]);out.mkdir(parents=True,exist_ok=True)
arm=bpy.data.objects['Armature'];obj=bpy.data.objects['Base mesh']
arm.data.pose_position='REST'
for mod in obj.modifiers:
 if mod.type=='ARMATURE': mod.show_viewport=False;mod.show_render=False
bpy.context.view_layer.update()
dg=bpy.context.evaluated_depsgraph_get();ev=obj.evaluated_get(dg);me=ev.to_mesh(preserve_all_data_layers=True,depsgraph=dg);me.calc_loop_triangles()
axis=Matrix(((1,0,0,0),(0,0,1,0),(0,-1,0,0),(0,0,0,1)))
world=axis@obj.matrix_world;nm=world.to_3x3().inverted().transposed()
names=[b.name for b in arm.data.bones];ids={n:i for i,n in enumerate(names)}
parents=[ids[b.parent.name] if b.parent else -1 for b in arm.data.bones]
bind=[axis@arm.matrix_world@b.matrix_local for b in arm.data.bones]
local=[bind[p].inverted()@m if p>=0 else m for p,m in zip(parents,bind)]
weights={};original_weights={};maxdrop=0
for v in me.vertices:
 ws=sorted([(ids[obj.vertex_groups[g.group].name],g.weight) for g in v.groups if obj.vertex_groups[g.group].name in ids and g.weight>0],key=lambda w:-w[1])
 if not ws: raise ValueError('Unweighted vertex '+str(v.index))
 total=sum(w for b,w in ws);original_weights[v.index]=[[b,w] for b,w in ws];maxdrop=max(maxdrop,sum(w for b,w in ws[4:])/total);ws=ws[:4];total=sum(w for b,w in ws);weights[v.index]=[[b,w/total] for b,w in ws]
verts=[];triangles=[];lookup={};uv=me.uv_layers.active
for tri in me.loop_triangles:
 indices=[]
 for li in tri.loops:
  vi=me.loops[li].vertex_index;p=world@me.vertices[vi].co;n=(nm@me.corner_normals[li].vector).normalized();t=list(uv.data[li].uv) if uv else [0,0];t=[t[0],1-t[1]]
  values=tuple(p)+tuple(n)+tuple(t);key=(vi,struct.pack('<8f',*values))
  if key not in lookup:lookup[key]=len(verts);verts.append(dict(position=list(p),normal=list(n),uv=t,weights=weights[vi],source_vertex=vi,original_weights=original_weights[vi]))
  indices.append(lookup[key])
 triangles.append(indices if world.to_3x3().determinant()>0 else [indices[0],indices[2],indices[1]])
assert 0<len(verts)<65536
assert all(math.isfinite(x) for v in verts for k in ('position','normal','uv') for x in v[k])
def chunk(k,d):return struct.pack('<HI',k,len(d)+6)+d
indices=[i for t in triangles for i in t]
decl=b''.join(chunk(0x5110,struct.pack('<5H',0,t,s,o,0)) for t,s,o in [(2,1,0),(2,4,12),(1,7,24)])
data=b''.join(struct.pack('<8f',*(v['position']+v['normal']+v['uv'])) for v in verts)
geo=chunk(0x5000,struct.pack('<I',len(verts))+chunk(0x5100,decl)+chunk(0x5200,struct.pack('<HH',0,32)+chunk(0x5210,data)))
sub=chunk(0x4000,b'human_skin\n'+struct.pack('<?I?',False,len(indices),False)+struct.pack('<'+'H'*len(indices),*indices)+geo+chunk(0x4010,struct.pack('<H',4)))
lo=[min(v['position'][k] for v in verts) for k in range(3)];hi=[max(v['position'][k] for v in verts) for k in range(3)];radius=max(math.sqrt(sum(x*x for x in v['position'])) for v in verts)
mesh=struct.pack('<H',0x1000)+b'[MeshSerializer_v1.8]\n'+chunk(0x3000,b'\0'+sub+chunk(0x9000,struct.pack('<7f',*(lo+hi+[radius])))+chunk(0xA000,chunk(0xA100,b'\0\0human\n'))+chunk(0xB000,b''))
(out/'human.mesh').write_bytes(mesh)
(out/'human.material').write_text('material human_skin\n{\n technique\n {\n  pass\n  {\n   ambient 0.8 0.8 0.8\n   diffuse 0.8 0.8 0.8\n   specular 0.15 0.15 0.15\n  }\n }\n}\n')
side=dict(format='YMSKE-human-skin-1',mesh='human.mesh',mesh_sha256=hashlib.sha256(mesh).hexdigest(),matrix_layout='row-major 3x4, column vectors',coordinate_transform='Blender (x,y,z) -> GRE (x,z,-y), original scale and origin',names=names,parents=parents,bind_local=[[list(r) for r in m][:3] for m in local],tails=[list(b.matrix_local.inverted()@b.tail_local) for b in arm.data.bones],vertices=verts,triangles=triangles,max_weight_drop=maxdrop,animations=[])
(out/'human.ske.json').write_text(json.dumps(side,ensure_ascii=False,separators=(',',':'))+'\n')
report=dict(source='Lowpolymesh_Eliber.blend',source_object=obj.name,vertices=len(verts),source_vertices=len(me.vertices),triangles=len(triangles),bones=len(names),max_weight_drop=maxdrop,bounds_min=lo,bounds_max=hi,texture_images=0,animations=0,mesh_sha256=side['mesh_sha256'])
(out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
# Exact loader comparison fixture, kept outside the deliverable.
Path('/tmp/gre-human-export/expected.bin').write_bytes(struct.pack('<II',len(verts),len(triangles))+data+struct.pack('<'+'H'*len(indices),*indices))
print('EXPORTED',json.dumps(report),flush=True)
ev.to_mesh_clear()

import os
sys.stdout.flush();os._exit(0)
