import com.ibm.mq.MQC;
import com.ibm.mq.MQEnvironment;
import com.ibm.mq.MQException;
import com.ibm.mq.MQGetMessageOptions;
import com.ibm.mq.MQMessage;
import com.ibm.mq.MQPutMessageOptions;
import com.ibm.mq.MQQueue;
import com.ibm.mq.MQQueueManager;

/*
 * 可以在MQ的资源管理器的某一个队列上放入测试消息、浏览消息等
 * 可以放入多条消息，按先进先出的方式取得
 */
public class MQTest {

	private String qManager;// QueueManager名

	private MQQueueManager qMgr;

	private MQQueue qQueue;

	String HOST_NAME;

	int PORT = 0;

	String Q_NAME;

	String CHANNEL;

	int CCSID;

	String Msg;

	public void init() {

		try {
			HOST_NAME = "182.119.137.121";// Hostname或IP
			PORT = 1415;// 要有一个侦听器，处于活动状态，且监听1414端口
			qManager = "QM1";
			Q_NAME = "Q1";// Q1是一个本地队列
			CHANNEL = "QM1CHL";// QM_APPLE上要建一个名为DC.SVRCONN的服务器连接通道
			CCSID = 1381; // 表示是简体中文，
			// CCSID的值在AIX上一般设为1383，如果要支持GBK则设为1386，在WIN上设为1381。

			MQEnvironment.hostname = HOST_NAME; // 安裝MQ所在的ip address
			MQEnvironment.port = PORT; // TCP/IP port

			MQEnvironment.channel = CHANNEL;
			MQEnvironment.CCSID = CCSID;

			qMgr = new MQQueueManager(qManager);

			int qOptioin = MQC.MQOO_INPUT_AS_Q_DEF | MQC.MQOO_INQUIRE | MQC.MQOO_OUTPUT;

			qQueue = qMgr.accessQueue(Q_NAME, qOptioin);

		} catch (MQException e) {
			System.out.println("A WebSphere MQ error occurred : Completion code " + e.completionCode + " Reason Code is " + e.reasonCode);
		}
	}

	void finalizer() {
		try {
			qQueue.close();
			qMgr.disconnect();
		} catch (MQException e) {
			System.out.println("A WebSphere MQ error occurred : Completion code " + e.completionCode + " Reason Code is " + e.reasonCode);
		}
	}

	/*
	 * 取过一次，下次就没有了
	 */
	public void GetMsg() {
		try {
			MQMessage retrievedMessage = new MQMessage();

			MQGetMessageOptions gmo = new MQGetMessageOptions();
			gmo.options += MQC.MQPMO_SYNCPOINT;

			qQueue.get(retrievedMessage, gmo);

			int length = retrievedMessage.getDataLength();

			byte[] msg = new byte[length];

			retrievedMessage.readFully(msg);

			String sMsg = new String(msg);
			System.out.println(sMsg);

		} catch (RuntimeException e) {
			e.printStackTrace();
		} catch (MQException e) {
			if (e.reasonCode != 2033) // 没有消息
			{
				e.printStackTrace();
				System.out.println("A WebSphere MQ error occurred : Completion code " + e.completionCode + " Reason Code is " + e.reasonCode);
			}
		} catch (java.io.IOException e) {
			System.out.println("An error occurred whilst to the message buffer " + e);
		}
	}

	public void SendMsg(byte[] qByte) {
		try {
			MQMessage qMsg = new MQMessage();
			qMsg.write(qByte);
			MQPutMessageOptions pmo = new MQPutMessageOptions();

			qQueue.put(qMsg, pmo);

			System.out.println("The message is sent!");
			System.out.println("\tThe message is " + new String(qByte, "GBK"));
		} catch (MQException e) {
			System.out.println("A WebSphere MQ error occurred : Completion code " + e.completionCode + " Reason Code is " + e.reasonCode);
		} catch (java.io.IOException e) {
			System.out.println("An error occurred whilst to the message buffer " + e);
		}

	}

	/**
	 * @param args
	 */
	public static void main(String[] args) {

		MQTest mqst = new MQTest();
		mqst.init();
		try {
			mqst.SendMsg("你好,Webshpere MQ 7.5!".getBytes("GBK"));
			mqst.GetMsg();
		} catch (Exception e) {
			e.printStackTrace();
		}
		mqst.finalizer();
	}

}
