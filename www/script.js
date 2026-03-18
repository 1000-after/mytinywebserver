// 测试JavaScript文件
console.log("🎯 Web服务器JavaScript文件加载成功！");

//显示当前时间
function updateTime(){
    const now = new Date();
    const timeElement = document.getElementById('current-time');
    if(timeElement){
        // 修复：textContext → textContent
        timeElement.textContent = now.toLocaleString('zh-CN');
    }
}

//页面加载完成后执行
document.addEventListener('DOMContentLoaded', function(){
    console.log("📄 页面加载完成");
    updateTime();

    //每秒钟更新时间
    setInterval(updateTime, 1000);
    
    //添加点击事件
    const buttons = document.querySelectorAll('.test-btn');
    buttons.forEach(btn =>{
        btn.addEventListener('click', function(e){
            // 修复：&{} → ${}
            console.log(`点击了: ${this.textContent}`);
        });
    });

    //显示浏览器信息
    const browserInfo = document.getElementById('browser-info');
    if(browserInfo){
        // 修复：所有&{} → ${}
        browserInfo.innerHTML = `
        <p><strong>用户代理:</strong> ${navigator.userAgent}</p>
        <p><strong>语言:</strong> ${navigator.language}</p>
        <p><strong>在线状态:</strong> ${navigator.onLine ? '在线' : '离线'} </p>
        `;
    }
});