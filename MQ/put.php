<?php

$mqcno = array(
	'Version' => MQSERIES_MQCNO_VERSION_2,
	'Options' => MQSERIES_MQCNO_STANDARD_BINDING,
	'MQCD' => array(
		'ChannelName' => 'QM1CHL',
		'ConnectionName' => '182.119.137.121(1415)',
		'TransportType' => MQSERIES_MQXPT_TCP,
	)
);

mqseries_connx('QM1', $mqcno, $conn, $comp_code, $reason);
if ($comp_code !== MQSERIES_MQCC_OK) {		
	printf("CONNX CompCode:%d Reason:%d Text:%s\n", $comp_code, $reason, mqseries_strerror($reason));
	exit;
}
$mqods = array('ObjectName' => 'Q1');
mqseries_open(
	$conn,
	$mqods,
	MQSERIES_MQOO_INPUT_AS_Q_DEF | MQSERIES_MQOO_FAIL_IF_QUIESCING | MQSERIES_MQOO_OUTPUT,
	$obj,
	$comp_code,
	$reason);
if ($comp_code !== MQSERIES_MQCC_OK) {
	printf("OPEN CompCode:%d Reason:%d Text:%s\n", $comp_code, $reason, mqseries_strerror($reason));
	exit;
}

$md = array(
	'Version' => MQSERIES_MQMD_VERSION_1,
	'Expiry' => MQSERIES_MQEI_UNLIMITED,
	'Report' => MQSERIES_MQRO_NONE,
	'MsgType' => MQSERIES_MQMT_DATAGRAM,
	'Format' => MQSERIES_MQFMT_STRING,
	'Priority' => 1,
	'Persistence' => MQSERIES_MQPER_PERSISTENT
);

$pmo = array('Options' => MQSERIES_MQPMO_NEW_MSG_ID);
mqseries_put(
	$conn, 
	$obj,
	$md,
	$pmo,
	'Ping '.date('Y-m-d H:i:s'),
	$comp_code,
	$reason
);
	
if ($comp_code !== MQSERIES_MQCC_OK) {
	printf("PUT CompCode:%d Reason:%d Text:%s\n", $comp_code, $reason, mqseries_strerror($reason));
}

mqseries_close($conn, $obj, MQSERIES_MQCO_NONE, $comp_code, $reason);
if ($comp_code !== MQSERIES_MQCC_OK) {
	printf("CLOSE CompCode:%d Reason:%d Text:%s\n", $comp_code, $reason, mqseries_strerror($reason));
}
mqseries_disc($conn, $comp_code, $reason);
if ($comp_code !== MQSERIES_MQCC_OK) {
	printf("DISC CompCode:%d Reason:%d Text:%s\n", $comp_code, $reason, mqseries_strerror($reason));
}

echo "done\n";
