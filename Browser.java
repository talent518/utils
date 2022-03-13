import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.EventQueue;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.net.URISyntaxException;

import javax.swing.BorderFactory;
import javax.swing.JButton;
import javax.swing.JFrame;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JTextField;
import javax.swing.WindowConstants;

import javafx.application.Platform;
import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.concurrent.Worker.State;
import javafx.embed.swing.JFXPanel;
import javafx.scene.Scene;
import javafx.scene.web.WebView;

public class Browser extends JFrame {
	private static final long serialVersionUID = 1220800278487283243L;

	WebView webView = null;
	JFXPanel jFXPanel = new JFXPanel();
	JPanel controlPanel = new JPanel();
	JTextField urlField = new JTextField();
	JButton goButton = new JButton("GO");

	public Browser(String url) {
		super();

		setTitle("WebView Browser");
		setLayout(new BorderLayout());
		setSize(800, 600);
		setDefaultCloseOperation(WindowConstants.DO_NOTHING_ON_CLOSE);
		setLocationRelativeTo(null);

		add(jFXPanel, BorderLayout.CENTER);
		add(controlPanel, BorderLayout.NORTH);

		urlField.setBorder(BorderFactory.createLineBorder(Color.gray));
		urlField.setText("http://www.baidu.com");
		goButton.addMouseListener(new MouseAdapter() {
			@Override
			public void mouseClicked(MouseEvent e) {
				String urlString = urlField.getText();
				gotoURL(urlString);
			}
		});

		controlPanel.setLayout(new BorderLayout(5, 5));
		controlPanel.setBorder(BorderFactory.createEmptyBorder(5, 5, 5, 5));
		controlPanel.add(urlField, BorderLayout.CENTER);
		controlPanel.add(goButton, BorderLayout.EAST);

		addWindowListener(new WindowAdapter() {
			@Override
			public void windowClosing(WindowEvent e) {
				if (JOptionPane.showConfirmDialog(null, "退出", "你确定退出吗？", JOptionPane.YES_NO_OPTION, JOptionPane.QUESTION_MESSAGE) == JOptionPane.YES_OPTION) {
					System.exit(0);
				}
			}
		});

		Platform.runLater(new Runnable() {
			@Override
			public void run() {
				webView = new WebView();
				jFXPanel.setScene(new Scene(webView));
				webView.getEngine().getLoadWorker().stateProperty().addListener(new ChangeListener<State>() {
					@Override
					public void changed(ObservableValue<? extends State> observable, State oldValue, State newValue) {
						System.out.println("new = " + newValue + ", old = " + oldValue);

						urlField.setText(webView.getEngine().getLocation());

						String title = webView.getEngine().getTitle();
						if (title == null || title.length() == 0) {
							title = "WebView Browser";
						} else {
							title += " - WebView Browser";
						}
						setTitle(title);
					}
				});
				webView.getEngine().load(url);
			}
		});
	}

	public void gotoURL(String url) {
		urlField.setText(url);
		Platform.runLater(new Runnable() {
			@Override
			public void run() {
				webView.getEngine().load(url);
			}
		});
	}

	/**
	 * @param args the command line arguments
	 * @throws URISyntaxException
	 */
	public static void main(String[] args) {
		EventQueue.invokeLater(() -> {
			new Browser(args.length > 0 ? args[0] : "https://www.baidu.com/").setVisible(true);
		});
	}

}
