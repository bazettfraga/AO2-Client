#ifndef DIRECT_CONNECT_DIALOG_H
#define DIRECT_CONNECT_DIALOG_H

#include <QDialog>

class QLabel;
class QSpinBox;
class QLineEdit;
class QPushButton;
class QComboBox;
class NetworkManager;

class DirectConnectDialog : public QDialog {
    Q_OBJECT
public:
    DirectConnectDialog(NetworkManager* p_net_manager);
    ~DirectConnectDialog() = default;

private slots:
    void onConnectPressed();
    void onServerConnected();

private:
    NetworkManager* net_manager;

    QComboBox* ui_direct_protocol_box;
    QLineEdit* ui_direct_hostname_edit;
    QSpinBox* ui_direct_port_box;

    QPushButton* ui_direct_connect_button;
    QPushButton* ui_direct_cancel_button;

    QWidget* ui_widget;

    const int TCP_INDEX = 0;
    const QString DEFAULT_UI = ":/resource/ui/direct_connect_dialog.ui";;

};

#endif // DIRECT_CONNECT_DIALOG_H
