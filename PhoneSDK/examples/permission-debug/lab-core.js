export const PREVIEW_LIMIT = 1024 * 1024;
export async function readBounded(stream, limit = PREVIEW_LIMIT) {
  const reader = stream.getReader(); const chunks=[]; let size=0;
  try {
    while (size < limit) {
      const {done,value}=await reader.read(); if(done) break;
      const bytes=value instanceof Uint8Array ? value : new Uint8Array(value);
      const chunk=bytes.subarray(0,limit-size); chunks.push(chunk);size+=chunk.length;
    }
  } finally { try {await reader.cancel();} finally {reader.releaseLock();} }
  const bytes=new Uint8Array(size);let offset=0;
  for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.length;}
  return bytes;
}
export function makeToneWav() {
  const rate=16000, count=rate*2, bytes=new Uint8Array(44+count*2), v=new DataView(bytes.buffer);
  const text=(offset,s)=>{for(let i=0;i<s.length;i++)v.setUint8(offset+i,s.charCodeAt(i));};
  text(0,'RIFF');v.setUint32(4,36+count*2,true);text(8,'WAVE');text(12,'fmt ');
  v.setUint32(16,16,true);v.setUint16(20,1,true);v.setUint16(22,1,true);v.setUint32(24,rate,true);
  v.setUint32(28,rate*2,true);v.setUint16(32,2,true);v.setUint16(34,16,true);text(36,'data');v.setUint32(40,count*2,true);
  for(let i=0;i<count;i++){
    const envelope=Math.min(1,i/800,(count-i)/800);
    v.setInt16(44+i*2,Math.sin(2*Math.PI*(i<rate?440:660)*i/rate)*envelope*6000,true);
  }
  return bytes;
}
export function gray4Pattern(width=120,height=70){
 const stride=Math.ceil(width/2),bytes=new Uint8Array(stride*height);
 for(let y=0;y<height;y++)for(let x=0;x<width;x++){
   const shade=(Math.floor(x/10)+Math.floor(y/10))%2?15:3;
   bytes[y*stride+(x>>1)]|=shade<<(x%2===0?4:0);
 }
 return {width,height,stride,bytes};
}
export function hexBytes(input){
 const hex=input.replace(/\s/g,'');
 if(!hex || hex.length%2 || !/^[0-9a-f]+$/i.test(hex) || hex.length>2048)throw new Error('请输入 1–1024 字节的十六进制数据');
 return Uint8Array.from(hex.match(/../g),v=>parseInt(v,16));
}
export function validateHttpUrl(value){
 const url=new URL(value);
 if(!['http:','https:'].includes(url.protocol)||url.username||url.password)throw new Error('仅支持不带用户名密码的 HTTP(S) 地址');
 return url.href;
}
