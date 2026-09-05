import re
from google.protobuf import descriptor_pb2, descriptor_pool, message_factory
def varint(b,i):
    r=0; s=0
    while True:
        c=b[i]; r|=(c&0x7f)<<s; i+=1
        if c<0x80: return r,i
        s+=7
        if s>63: raise ValueError
def blob_end(b,i0):
    i=i0
    while i<len(b):
        try: tag,j=varint(b,i)
        except Exception: break
        f=tag>>3; wt=tag&7
        if f==0 or f>12 or wt not in (0,2): break
        try:
            if wt==0: _,j=varint(b,j)
            else:
                ln,j=varint(b,j)
                if j+ln>len(b): break
                j+=ln
        except Exception: break
        i=j
    return i
def load(exe=r"C:\InfinityX\Games\Assetto Corsa EVO\AssettoCorsaEVO.exe"):
    d=open(exe,"rb").read(); seen={}; 
    for m in re.finditer(rb"\x0a[\x01-\x7f]([\x20-\x7e]{3,120}?\.proto)[\x12\x1a\x22\x2a\x32]", d):
        st=m.start(); ln=d[st+1]; name=d[st+2:st+2+ln]
        if not name.endswith(b".proto") or name in seen: continue
        try: fd=descriptor_pb2.FileDescriptorProto.FromString(d[st:blob_end(d,st)])
        except Exception: continue
        if fd.name==name.decode(): seen[fd.name]=fd
    pool=descriptor_pool.DescriptorPool()
    pending=dict(seen); added=set()
    while pending:
        prog=False
        for n,fd in list(pending.items()):
            if all(dep in added for dep in fd.dependency):
                try: pool.Add(fd)
                except Exception as e: pass
                added.add(n); del pending[n]; prog=True
        if not prog:
            for n,fd in list(pending.items()):
                try: pool.Add(fd)
                except Exception as e: print("pool add fail",n,e)
                added.add(n); del pending[n]
    return pool, seen
def msg_class(pool, fullname):
    return message_factory.GetMessageClass(pool.FindMessageTypeByName(fullname))
