import {normalizePermissions,authorize} from './permission-policy.js';
// Host-owned debug consent; no consent inferred from a plugin manifest.
export async function requestDebugApprovals(permissions) {
  const declarations = normalizePermissions(permissions);
  return new Promise(resolve=>{
    const dialog = document.createElement('dialog');
    const heading = document.createElement('h2'); heading.textContent='插件权限调试授权';
    const notice = document.createElement('p');
    notice.textContent='仅用于开发调试：定位为固定模拟坐标；眼镜录音暂不支持。network 与 H5 自动播放只记录授权，不构成原生 WebView 隔离验证。不要加载不可信插件。';
    dialog.append(heading,notice);
    const boxes=[];
    for (const p of declarations) {
      const row=document.createElement('label'); row.style.display='block';
      const box=document.createElement('input'); box.type='checkbox'; box.checked=false;
      const description=p.name+' / '+(p.required?'必需':'可选')+(p.scope?' '+JSON.stringify(p.scope):'');
      row.append(box,document.createTextNode(description)); dialog.append(row); boxes.push([p,box]);
    }
    const error=document.createElement('p'); dialog.append(error);
    const start=document.createElement('button'); start.textContent='按所选权限启动';
    const cancel=document.createElement('button'); cancel.textContent='取消';
    const finish=value=>{dialog.close();dialog.remove();resolve(value);};
    start.onclick=()=>{
      const approvals=Object.fromEntries(boxes.filter(([,box])=>box.checked).map(([p])=>[p.name,p.scope??null]));
      try {authorize('runtime.ready',{},declarations,approvals);finish(approvals);}
      catch(e){error.textContent='必需权限尚未全部同意，不能启动。';}
    };
    cancel.onclick=()=>finish(null);
    dialog.addEventListener('cancel',e=>{e.preventDefault();finish(null);});
    dialog.append(start,cancel);document.body.append(dialog);dialog.showModal();
  });
}
