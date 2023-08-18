const net = require('net');
const now = function() {
	const d = new Date();
	return d.toLocaleTimeString() + '.' + (d.getTime() % 1000).toString().padStart(3, '0');
};
const srv = net.createServer(function(conn) {
	const addr = conn.remoteAddress + ':' + conn.remotePort;
	console.log('Connected', addr);

	conn.on('data', function(data) {
		console.log('data', addr, data.toString());
		conn.write(now(), function(e) {
			if(e) console.error(e);
		});
	});
	conn.on('error', function(e) {
		console.log('Error', addr, e.message);
	});
	conn.on('close', function() {
		console.log('Closed', addr);
		
		conn.destroy();
	});
});

srv.listen(3888, '0.0.0.0', function() {
	console.log('listen in 0.0.0.0:3888');
});

