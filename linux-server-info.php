<?php

$ips = array('self'=>$_SERVER['SERVER_ADDR'], '127'=>'127.0.0.1');

$ipKey = array_search($_SERVER['SERVER_ADDR'], $ips);

$retObject = array(
	'https' => max(0, exec('ss state established \'sport = :http\' | wc -l') - 1),
	'mysqls' => 0,
	'memcaches' => 0,
	'cpuCores' => exec('cat /proc/cpuinfo | grep processor | wc -l') + 0,
);

if($ipKey === 'cache') {
	$retObject['memcaches'] = max(0, exec('ss state established \'sport = :11211\' | wc -l') - 1);
} else {
	$retObject['memcaches'] = max(0, exec('ss state established \'dport = :11211\' dst 182.241.129.27 | wc -l') - 1);
}

if($ipKey === 'db' || $ipKey === 'report') {
	$retObject['mysqls'] = max(0, exec('ss state established \'sport = :3306\' | wc -l') - 1);
} else {
	$retObject['mysqls'] = max(0, exec('ss state established \'dport = :3306\' dst 182.241.21.105 | wc -l') - 1);
}

$retObject['cpu'] = array();

list(, $retObject['cpu']['user'], $retObject['cpu']['nice'], $retObject['cpu']['sys'], $retObject['cpu']['idle'], $retObject['cpu']['iowait'], $retObject['cpu']['irq'], $retObject['cpu']['softirq'], $retObject['cpu']['stolen'], $retObject['cpu']['guest']) = sscanf(@file_get_contents('/proc/stat'), '%s %d %d %d %d %d %d %d %d %d');

//内存
$str = @file("/proc/meminfo");
if($str) {
	$str = implode("", $str);
	preg_match_all("/MemTotal\s{0,}\:+\s{0,}([\d\.]+).+?MemFree\s{0,}\:+\s{0,}([\d\.]+).+?Cached\s{0,}\:+\s{0,}([\d\.]+).+?SwapTotal\s{0,}\:+\s{0,}([\d\.]+).+?SwapFree\s{0,}\:+\s{0,}([\d\.]+)/s", $str, $buf);
	preg_match_all("/Buffers\s{0,}\:+\s{0,}([\d\.]+)/s", $str, $buffers);
	$retObject['memTotal'] = round($buf[1][0]/1024, 2);
	$retObject['memFree'] = round($buf[2][0]/1024, 2);
	$retObject['memBuffers'] = round($buffers[1][0]/1024, 2);
	$retObject['memCached'] = round($buf[3][0]/1024, 2);
	$retObject['memUsed'] = $retObject['memTotal']-$retObject['memFree'];
	$retObject['memPercent'] = (floatval($retObject['memTotal'])!=0)?round($retObject['memUsed']/$retObject['memTotal']*100,2):0;
	$retObject['memRealUsed'] = $retObject['memTotal'] - $retObject['memFree'] - $retObject['memCached'] - $retObject['memBuffers']; //真实内存使用
	$retObject['memRealFree'] = $retObject['memTotal'] - $retObject['memRealUsed']; //真实空闲
	$retObject['memRealPercent'] = (floatval($retObject['memTotal'])!=0)?round($retObject['memRealUsed']/$retObject['memTotal']*100,2):0; //真实内存使用率
	$retObject['memCachedPercent'] = (floatval($retObject['memCached'])!=0)?round($retObject['memCached']/$retObject['memTotal']*100,2):0; //Cached内存使用率
	$retObject['swapTotal'] = round($buf[4][0]/1024, 2);
	$retObject['swapFree'] = round($buf[5][0]/1024, 2);
	$retObject['swapUsed'] = round($retObject['swapTotal']-$retObject['swapFree'], 2);
	$retObject['swapPercent'] = (floatval($retObject['swapTotal'])!=0)?round($retObject['swapUsed']/$retObject['swapTotal']*100,2):0;
}

if(isset($_REQUEST['isprivate']) && $_REQUEST['isprivate']) {
	header('Content-Type: application/json');

	exit(json_encode($retObject));
	exit;
}

$mh = curl_multi_init(); // init the curl Multi
$aCurlHandles = array(); // create an array for the individual curl handles
$retObjects = array();

foreach($ips as $key=>$ip) {
	if($ip == $_SERVER['SERVER_ADDR']) {
		$retObjects[$key] = $retObject;
	} else {
		$retObjects[$key] = array();
		$url = 'http://' . $ip . $_SERVER['SCRIPT_NAME'] . '?isprivate=1';

		$ch = curl_init(); // init curl, and then setup your options
		curl_setopt($ch, CURLOPT_URL, $url);
		curl_setopt($ch, CURLOPT_RETURNTRANSFER,1); // returns the result - very important
		curl_setopt($ch, CURLOPT_HEADER, 0); // no headers in the output

		$aCurlHandles[$key] = $ch;
		curl_multi_add_handle($mh, $ch);
	}
}

$active = null;
//execute the handles
do {
	$mrc = curl_multi_exec($mh, $active);
} while ($mrc == CURLM_CALL_MULTI_PERFORM);

while ($active && $mrc == CURLM_OK) {
	if (curl_multi_select($mh) != -1) {
		do {
			$mrc = curl_multi_exec($mh, $active);
		} while ($mrc == CURLM_CALL_MULTI_PERFORM);
	}
}

/* This is the relevant bit */
// iterate through the handles and get your content
foreach ($aCurlHandles as $key=>$ch) {
	$json = curl_multi_getcontent($ch); // get the content
	if($json && is_array($json=json_decode($json, true))) {
		$retObjects[$key] = $json;
	}
	// do what you want with the HTML
	curl_multi_remove_handle($mh, $ch); // remove the handle (assuming  you are done with it);
}
/* End of the relevant bit */

curl_multi_close($mh); // close the curl multi handler

if(isset($_REQUEST['isajax']) && $_REQUEST['isajax']) {
	header('Content-Type: application/json');

	echo 'jsonCallback(', json_encode($retObjects), ');removeElemById("j-script-', $_REQUEST['time'], '");';
	exit;
}

$https = $mysqls = 0;
foreach($retObjects as $k=>$v) {
	$https += $v['https'];
	$mysqls += $v['mysqls'];
}

header('Content-Type: text/html; charset=utf-8');
?><!doctype html>
<html lang="en">
<head>
	<meta charset="UTF-8">
	<title>服务器信息</title>
	<style type="text/css">
	body{text-align:center;padding:20px;font-size:14px;font-weight:bold;font-family:'微软雅黑','MS YaHei','黑体';}
	table{border-spacing:0px;border-collapse:collapse;}
	th,td{padding:5px;white-space:nowrap;}
	th{color:#000;background:#eee;}
	th span,td{color:#F60;}
	.list{width:100%;}
	.list th,.list td{text-align:right;width:5%;border-collapse:collapse;border:1px #ccc solid;}
	.list th.ip{width:2.4em;}
	.list .c{text-align:center;}
	.list .oven{background:#f6f6f6;}
	</style>
</head>
<body>
	<table class="list" align="center" valign="middle">
		<thead>
			<tr>
				<th rowspan="2" class="ip">IP</th>
				<th colspan="3" class="c">连接数</th>
				<th colspan="10" class="c">CPU使用情况(单位：百分比%)</th>
				<th colspan="6" class="c">内存使用情况(单位：百分比%)</th>
			</tr>
			<tr>
				<th>https(<span id="j-https" title="app合">0</span>)</th>
				<th>mysqls(<span id="j-mysqls" title="app合">0</span>)</th>
				<th>memcaches(<span id="j-memcaches" title="app合">0</span>)</th>
				<th>cores</th>
				<th>user</th>
				<th>nice</th>
				<th>sys</th>
				<th>idle</th>
				<th>iowait</th>
				<th>irq</th>
				<th>softirq</th>
				<th>stolen</th>
				<th>guest</th>
				<th>total</th>
				<th>percent</th>
				<th>realPercent</th>
				<th>cachedPercent</th>
				<th>swapTotal</th>
				<th>swapPercent</th>
			</tr>
		</thead>
		<tbody id="j-each-info"></tbody>
	</table>
	<script type="text/javascript">
	function IntervalCallback() {
		var time = new Date().getTime();
		var elem = document.createElement('script');
		elem.id = 'j-script-' + time;
		elem.type = 'text/javascript';
		elem.src = '?isajax=1&time=' + time;
		document.body.appendChild(elem);
	}
	function removeElemById(id) {
		var elem = document.getElementById(id);
		if(elem) {
			if(elem.parentElement) {
				elem.parentElement.removeChild(elem);
			} else {
				elem.parentNode.removeChild(elem);
			}
		}
	}
	function setHtmlById(id, html) {
		var elem = document.getElementById(id);
		if(elem) {
			elem.innerHTML = html;
		}
	}
	function forEach(obj, callback) {
		var k, v;
		for(k in obj) {
			if(callback.call(obj, k, obj[k]) === false) {
				return;
			}
		}
	}
	function jsonCallback(json) {
		setTimeout(IntervalCallback, 1000);
		
		var eachHtml = '', html, cpu, sum, sum1, sum2, i = 0, https = 0, mysqls = 0, memcaches = 0;
		forEach(json, function(k, v) {
			$v = window.retObjects[k];

			if(/^app\d+$/.test(k)) {
				https += v.https;
				mysqls += v.mysqls;
				memcaches += v.memcaches;
			}

			cpu = {};
			sum1 = 0;
			sum2 = 0;
			forEach(v.cpu, function(k2, v2) {
				cpu[k2] = (v2 - $v.cpu[k2]);
				sum1 += v2;
				sum2 += $v.cpu[k2];
			});

			sum = Math.max(1, sum1 - sum2);
			html = '';
			forEach(cpu, function(k2, v2) {
				var str = new String(Math.round(v2*1000/sum)/10);
				if(str.indexOf('.') == -1) {
					str += '.0';
				}
				html = '<td>' + str + '</td>' + html;
			});
			
			html = '<td>' + v.cpuCores + '</td>' + html;

			html += '<td>' + (v.memTotal<1024 ? v.memTotal + 'M' : Math.round((v.memTotal*1000/1024)/1000) + 'G') + '</td>';
			html += '<td>' + v.memPercent + '</td>';
			html += '<td>' + v.memRealPercent + '</td>';
			html += '<td>' + v.memCachedPercent + '</td>';
			html += '<td>' + (v.swapTotal<1024 ? v.swapTotal + 'M' : Math.round((v.swapTotal*1000/1024)/1000) + 'G') + '</td>';
			html += '<td>' + v.swapPercent + '</td>';

			eachHtml += '<tr' + (i++%2 ? ' class="oven"' : '') + '><th class="ip">' + k + '</th><td>' +v.https + '</td><td>' +v.mysqls + '</td><td>' +v.memcaches + '</td>' + html + '</tr>';
		});

		setHtmlById('j-each-info', eachHtml);
		setHtmlById('j-https', https);
		setHtmlById('j-mysqls', mysqls);
		setHtmlById('j-memcaches', memcaches);

		window.retObjects = json;
	}
	window.retObjects = <?php echo json_encode($retObjects);?>;
	jsonCallback(window.retObjects);
	</script>
</body>
</html>
