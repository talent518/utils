const fs = require('fs');
const path = require('path');
const dumpFile = path.join(process.env.HOME, '.pm2', 'dump.pm2');

if(!fs.existsSync(dumpFile)) return;

const data = JSON.parse(fs.readFileSync(dumpFile)).map(v=>{
	return {
		name: v.name,
		autorestart: v.autorestart,
		cron_restart: v.cron_restart,
		status: v.status,
		cwd: v.pm_cwd.replace(process.env.HOME + '/', '~/'),
		path: path.basename(v.pm_exec_path),
	};
});

data.sort((a,b)=>{
	let ret;

	ret = a.cwd.localeCompare(b.cwd);
	if(ret) return ret;

	if(a.autorestart !== b.autorestart) return a.autorestart ? -1 : 1;

	return a.name.localeCompare(b.name);
});

console.table(data);
