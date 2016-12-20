import com.ibm.mq.MQC;
import com.ibm.mq.MQEnvironment;
import com.ibm.mq.MQException;
import com.ibm.mq.MQGetMessageOptions;
import com.ibm.mq.MQMessage;
import com.ibm.mq.MQPutMessageOptions;
import com.ibm.mq.MQQueue;
import com.ibm.mq.MQQueueManager;

/*
 * ������MQ����Դ��������ĳһ�������Ϸ��������Ϣ�������Ϣ��
 * ���Է��������Ϣ�����Ƚ��ȳ��ķ�ʽȡ��
 */
public class MQTest {

	private String qManager;// QueueManager��

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
			HOST_NAME = "182.119.137.121";// Hostname��IP
			PORT = 1415;// Ҫ��һ�������������ڻ״̬���Ҽ���1414�˿�
			qManager = "QM1";
			Q_NAME = "Q1";// Q1��һ�����ض���
			CHANNEL = "QM1CHL";// QM_APPLE��Ҫ��һ����ΪDC.SVRCONN�ķ���������ͨ��
			CCSID = 1381; // ��ʾ�Ǽ������ģ�
			// CCSID��ֵ��AIX��һ����Ϊ1383�����Ҫ֧��GBK����Ϊ1386����WIN����Ϊ1381��

			MQEnvironment.hostname = HOST_NAME; // ���bMQ���ڵ�ip address
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
	 * ȡ��һ�Σ��´ξ�û����
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
			if (e.reasonCode != 2033) // û����Ϣ
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
			mqst.SendMsg("���,Webshpere MQ 7.5!".getBytes("GBK"));
			mqst.GetMsg();
		} catch (Exception e) {
			e.printStackTrace();
		}
		mqst.finalizer();
	}

}
