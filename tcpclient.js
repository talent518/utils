const net = require('net');
const now = function() {
	const d = new Date();
	return d.toLocaleTimeString() + '.' + (d.getTime() % 1000).toString().padStart(3, '0');
};
let stoped = false;
const newConn = function(i) {
	if(stoped) return;
	
	let addr, et = 0;
	const conn = new net.Socket();
	
	conn.on('error', function(e) {
		console.error('Error', i, e.message);
	});

	conn.on('data', function(data) {
		console.log('data', i, addr, data.toString());
		
		if(Date.now() >= et) conn.end();
		else conn.write('[' + i + '] ' + now());
	});
	conn.on('end', function() {
		console.log('Closed', i, addr);
		newConn(i);
	});

	conn.connect(3888, '127.0.0.1', function() {
		et = Date.now() + 1000;
		
		addr = conn.localAddress + ':' + conn.localPort;
		console.log('Connected', i, addr);
		
		conn.write('[' + i + '] ' + now());
	});
};

process.on('SIGINT', function() {
	stoped = true;
});

for(let i=0; i<10; i++) {
	newConn(i);
}

