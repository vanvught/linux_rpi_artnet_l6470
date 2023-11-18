async function refresh() {
  try {
    let data=await getJSON('rdm/portstatus')
    let tr=await Promise.all(
      data.map(item => getJSON('rdm/tod?' + item.port).then(response => ({ port: item.port, tod: response.tod })))
    );

    let h='<tr><th>Port</th><th>Direction</th><th>Status</th></tr>'
    data.forEach(item => {
      h+=`<tr><td>${item.port}</td><td>${item.direction}</td><td>${item.status}</td></tr>`
    });
  
    document.getElementById("idCfg").innerHTML=h

    try {
      let q=await getJSON('rdm/queue')
      h='';
      q.uid.forEach(uid => {
        h+=`<tr><td colspan="3">${uid}</td></tr>`
      });
    } catch (error){h=''}
    
    document.getElementById("idQue").innerHTML=h

    h=''
    let hdrs='<tr>'
    let tdd='<tr>'

    tr.sort((a, b) => {
      return data.findIndex(item => item.port === a.port) - data.findIndex(item => item.port === b.port);
    });

    tr.forEach(r => {
      hdrs+=`<th>${r.port}</th>`;
      tdd+='<td>';
      r.tod.forEach(tod => {
        tdd+=`${tod}<br/>`
      });
      tdd+='</td>'
    });

    hdrs+='</tr>'
    tdd+='</tr>'
    document.getElementById("idDis").innerHTML='<table>' + h + hdrs + tdd + '</table>'
  } catch (error){}
}