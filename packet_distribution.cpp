#include "aoapplication.h"

#include "lobby.h"
#include "courtroom.h"
#include "networkmanager.h"
#include "encryption_functions.h"
#include "hardware_functions.h"
#include "debug_functions.h"

#include <QDebug>

void AOApplication::ms_packet_received(AOPacket *p_packet)
{
  p_packet->net_decode();

  QString header = p_packet->get_header();
  QStringList f_contents = p_packet->get_contents();

  if (header != "CHECK")
    qDebug() << "R(ms):" << p_packet->to_string();

  if (header == "ALL")
  {
    server_list.clear();

    for (QString i_string : p_packet->get_contents())
    {
      server_type f_server;
      QStringList sub_contents = i_string.split("&");

      if (sub_contents.size() < 4)
      {
        qDebug() << "W: malformed packet";
        continue;
      }

      f_server.name = sub_contents.at(0);
      f_server.desc = sub_contents.at(1);
      f_server.ip = sub_contents.at(2);
      f_server.port = sub_contents.at(3).toInt();

      server_list.append(f_server);
    }

    if (lobby_constructed)
    {
      w_lobby->list_servers();
    }
  }
  else if (header == "CT")
  {
    QString message_line;

    if (f_contents.size() == 1)
      message_line = f_contents.at(0);
    else if (f_contents.size() >= 2)
      message_line = f_contents.at(0) + ": " + f_contents.at(1);
    else
      goto end;

    if (lobby_constructed)
    {
      w_lobby->append_chatmessage(message_line);
    }
    if (courtroom_constructed)
    {
      w_courtroom->append_ms_chatmessage(message_line);
    }
  }

  end:

  delete p_packet;
}

void AOApplication::server_packet_received(AOPacket *p_packet)
{
  p_packet->net_decode();

  QString header = p_packet->get_header();
  QStringList f_contents = p_packet->get_contents();
  QString f_packet = p_packet->to_string();

  if (header != "checkconnection")
    qDebug() << "R: " << f_packet;

  if (header == "decryptor")
  {
    if (f_contents.size() == 0)
      goto end;

    //you may ask where 322 comes from. that would be a good question.
    s_decryptor = fanta_decrypt(f_contents.at(0), 322).toUInt();

    QString f_hdid;
    f_hdid = get_hdid();

    AOPacket *hi_packet = new AOPacket("HI#" + f_hdid + "#%");
    send_server_packet(hi_packet);
  }
  else if (header == "ID")
  {
    if (f_contents.size() < 2)
      goto end;

    s_pv = f_contents.at(0).toInt();

    QString server_version = f_contents.at(1);

    if (server_version == "v1300.146")
    {
      encryption_needed = false;
      yellow_text_enabled = true;
      //still needs some tweaking to work
      prezoom_enabled = false;
      flipping_enabled = true;
      custom_objection_enabled = true;
      //improved loading disabled for now
      //improved_loading_enabled = true;
      improved_loading_enabled = false;
    }
    else
    {
      encryption_needed = true;
      yellow_text_enabled = false;
      prezoom_enabled = false;
      flipping_enabled = false;
      custom_objection_enabled = false;
      improved_loading_enabled = false;
    }
  }
  else if (header == "CT")
  {
    if (f_contents.size() < 2)
    {
      qDebug() << "W: malformed packet!";
      goto end;
    }

    QString message_line = f_contents.at(0) + ": " + f_contents.at(1);

    if (courtroom_constructed)
      w_courtroom->append_server_chatmessage(message_line);
  }
  else if (header == "PN")
  {
    if (f_contents.size() < 2)
      goto end;

    w_lobby->set_player_count(f_contents.at(0).toInt(), f_contents.at(1).toInt());
  }
  else if (header == "SI")
  {
    if (f_contents.size() < 3)
      goto end;

    char_list_size = f_contents.at(0).toInt();
    loaded_chars = 0;
    evidence_list_size = f_contents.at(1).toInt();
    loaded_evidence = 0;
    music_list_size = f_contents.at(2).toInt();
    loaded_music = 0;

    destruct_courtroom();
    construct_courtroom();

    QString window_title = "Attorney Online 2";
    int selected_server = w_lobby->get_selected_server();

    if (w_lobby->public_servers_selected)
    {
      if (selected_server >= 0 && selected_server < server_list.size())
        window_title += ": " + server_list.at(selected_server).name;
    }
    else
    {
      if (selected_server >= 0 && selected_server < favorite_list.size())
        window_title += ": " + favorite_list.at(selected_server).name;
    }

    w_courtroom->set_window_title(window_title);

    w_lobby->show_loading_overlay();
    w_lobby->set_loading_text("Loading");
    w_lobby->set_loading_value(0);

    AOPacket *f_packet;

    //AO2 loading disabled for now
    //if(improved_loading_enabled)
    //  f_packet = new AOPacket("RC#%");
    //else
      f_packet = new AOPacket("askchar2#%");

    send_server_packet(f_packet);
  }
  else if (header == "CI")
  {
    if (!courtroom_constructed)
      goto end;

    for (int n_element = 0 ; n_element < f_contents.size() ; n_element += 2)
    {
      if (f_contents.at(n_element).toInt() != loaded_chars)
        break;

      //this means we are on the last element and checking n + 1 element will be game over so
      if (n_element == f_contents.size() - 1)
        break;

      QStringList sub_elements = f_contents.at(n_element + 1).split("&");
      if (sub_elements.size() < 2)
        break;

      char_type f_char;
      f_char.name = sub_elements.at(0);
      f_char.description = sub_elements.at(1);
      //temporary. the CharsCheck packet sets this properly
      f_char.taken = false;

      ++loaded_chars;

      w_lobby->set_loading_text("Loading chars:\n" + QString::number(loaded_chars) + "/" + QString::number(char_list_size));

      w_courtroom->append_char(f_char);
    }

    int total_loading_size = char_list_size + evidence_list_size + music_list_size;
    int loading_value = (loaded_chars / static_cast<double>(total_loading_size)) * 100;
    w_lobby->set_loading_value(loading_value);

    if (improved_loading_enabled)
      send_server_packet(new AOPacket("RE#%"));
    else
    {
      QString next_packet_number = QString::number(((loaded_chars - 1) / 10) + 1);
      send_server_packet(new AOPacket("AN#" + next_packet_number + "#%"));
    }

  }
  else if (header == "EI")
  {
    if (!courtroom_constructed)
      goto end;


    // +1 because evidence starts at 1 rather than 0 for whatever reason
    //enjoy fanta
    if (f_contents.at(0).toInt() != loaded_evidence + 1)
      goto end;

    if (f_contents.size() < 2)
      goto end;

    QStringList sub_elements = f_contents.at(1).split("&");
    if (sub_elements.size() < 4)
      goto end;

    evi_type f_evi;
    f_evi.name = sub_elements.at(0);
    f_evi.description = sub_elements.at(1);
    //no idea what the number at position 2 is. probably an identifier?
    f_evi.image = sub_elements.at(3);

    ++loaded_evidence;

    w_lobby->set_loading_text("Loading evidence:\n" + QString::number(loaded_evidence) + "/" + QString::number(evidence_list_size));

    w_courtroom->append_evidence(f_evi);

    int total_loading_size = char_list_size + evidence_list_size + music_list_size;
    int loading_value = ((loaded_chars + loaded_evidence) / static_cast<double>(total_loading_size)) * 100;
    w_lobby->set_loading_value(loading_value);

    QString next_packet_number = QString::number(loaded_evidence);
    send_server_packet(new AOPacket("AE#" + next_packet_number + "#%"));

  }
  else if (header == "EM")
  {
    if (!courtroom_constructed)
      goto end;

    for (int n_element = 0 ; n_element < f_contents.size() ; n_element += 2)
    {
      if (f_contents.at(n_element).toInt() != loaded_music)
        break;

      if (n_element == f_contents.size() - 1)
        break;

      QString f_music = f_contents.at(n_element + 1);

      ++loaded_music;

      w_lobby->set_loading_text("Loading music:\n" + QString::number(loaded_music) + "/" + QString::number(music_list_size));

      w_courtroom->append_music(f_music);
    }

    int total_loading_size = char_list_size + evidence_list_size + music_list_size;
    int loading_value = ((loaded_chars + loaded_evidence + loaded_music) / static_cast<double>(total_loading_size)) * 100;
    w_lobby->set_loading_value(loading_value);

    QString next_packet_number = QString::number(((loaded_music - 1) / 10) + 1);
    send_server_packet(new AOPacket("AM#" + next_packet_number + "#%"));
  }
  else if (header == "CharsCheck")
  {
    for (int n_char = 0 ; n_char < f_contents.size() ; ++n_char)
    {
      if (f_contents.at(n_char) == "-1")
        w_courtroom->set_taken(n_char, true);
      else
        w_courtroom->set_taken(n_char, false);
    }
  }
  //AO2 loading is temporarily disabled as it needs to be revised altogether
  /*
  else if (header == "SC")
  {
    if (!courtroom_constructed)
      goto end;

    for (int n_element = 0 ; n_element < f_contents.size() ; ++n_element)
    {
      QStringList sub_elements = f_contents.at(n_element).split("&");
      if (sub_elements.size() < 2)
        break;

      char_type f_char;
      f_char.name = sub_elements.at(0);
      f_char.description = sub_elements.at(1);
      //temporary. the TC packet sets this properly
      f_char.taken = false;

      ++loaded_chars;

      w_lobby->set_loading_text("Loading chars:\n" + QString::number(loaded_chars) + "/" + QString::number(char_list_size));

      w_courtroom->append_char(f_char);
    }

    int total_loading_size = char_list_size + evidence_list_size + music_list_size;
    int loading_value = (loaded_chars / static_cast<double>(total_loading_size)) * 100;
    w_lobby->set_loading_value(loading_value);

    send_server_packet(new AOPacket("RE#%"));
  }
  else if (header == "SE")
  {
    if (!courtroom_constructed)
      goto end;

    // +1 because evidence starts at 1 rather than 0 for whatever reason
    //enjoy fanta
    if (f_contents.at(0).toInt() != loaded_evidence + 1)
      goto end;

    if (f_contents.size() < 2)
      goto end;

    QStringList sub_elements = f_contents.at(1).split("&");
    if (sub_elements.size() < 4)
      goto end;

    evi_type f_evi;
    f_evi.name = sub_elements.at(0);
    f_evi.description = sub_elements.at(1);
    //no idea what the number at position 2 is. probably an identifier?
    f_evi.image = sub_elements.at(3);

    ++loaded_evidence;

    w_lobby->set_loading_text("Loading evidence:\n" + QString::number(loaded_evidence) + "/" + QString::number(evidence_list_size));

    w_courtroom->append_evidence(f_evi);

    int total_loading_size = char_list_size + evidence_list_size + music_list_size;
    int loading_value = ((loaded_chars + loaded_evidence) / static_cast<double>(total_loading_size)) * 100;
    w_lobby->set_loading_value(loading_value);

    send_server_packet(new AOPacket("RM#%"));
  }
  */
  else if (header == "DONE")
  {
    if (!courtroom_constructed)
      goto end;

    w_courtroom->set_char_select_page();

    w_courtroom->append_ms_chatmessage(w_lobby->get_chatlog());

    w_courtroom->set_mute_list();

    w_courtroom->show();

    destruct_lobby();
  }
  else if (header == "BN")
  {
    if (f_contents.size() < 1)
      goto end;

    if (courtroom_constructed)
      w_courtroom->set_background(f_contents.at(0));
  }
  //server accepting char request(CC) packet
  else if (header == "PV")
  {
    if (f_contents.size() < 3)
      goto end;

    if (courtroom_constructed)
      w_courtroom->enter_courtroom(f_contents.at(2).toInt());
  }
  else if (header == "MS")
  {
    if (courtroom_constructed)
      w_courtroom->handle_chatmessage(&p_packet->get_contents());
  }
  else if (header == "MC")
  {
    if (courtroom_constructed)
      w_courtroom->handle_song(&p_packet->get_contents());
  }
  else if (header == "RT")
  {
    if (f_contents.size() < 1)
      goto end;
    if (courtroom_constructed)
      w_courtroom->handle_wtce(f_contents.at(0));
  }
  else if (header == "HP")
  {
    if (courtroom_constructed && f_contents.size() > 1)
      w_courtroom->set_hp_bar(f_contents.at(0).toInt(), f_contents.at(1).toInt());
  }
  else if (header == "IL")
  {
    if (courtroom_constructed && f_contents.size() > 0)
      w_courtroom->set_ip_list(f_contents.at(0));
  }
  else if (header == "MU")
  {
    if (courtroom_constructed && f_contents.size() > 0)
      w_courtroom->set_mute(true, f_contents.at(0).toInt());
  }
  else if (header == "UM")
  {
    if (courtroom_constructed && f_contents.size() > 0)
      w_courtroom->set_mute(false, f_contents.at(0).toInt());
  }
  else if (header == "KK")
  {
    if (courtroom_constructed && f_contents.size() > 0)
    {
      int f_cid = w_courtroom->get_cid();
      int remote_cid = f_contents.at(0).toInt();

      if (f_cid != remote_cid && remote_cid != -1)
        goto end;

      call_notice("You have been kicked.");
      construct_lobby();
      destruct_courtroom();
    }

  }
  else if (header == "KB")
  {
    if (courtroom_constructed && f_contents.size() > 0)
      w_courtroom->set_ban(f_contents.at(0).toInt());
  }
  else if (header == "BD")
  {
    call_notice("You are banned on this server.");
  }
  else if (header == "AO2CHECK")
  {
    if (f_contents.size() < 1)
      goto end;

    QStringList version_contents = f_contents.at(0).split(".");

    if (version_contents.size() < 3)
      goto end;

    int f_release = version_contents.at(0).toInt();
    int f_major = version_contents.at(1).toInt();
    int f_minor = version_contents.at(2).toInt();

    //qDebug() << "local version: " << get_version_string();
    //qDebug() << "remote version: " << QString::number(f_release) << QString::number(f_major) << QString::number(f_minor);

    if (get_release() > f_release)
      goto end;
    else if (get_release() == f_release)
    {
      if (get_major_version() > f_major)
        goto end;
      else if (get_major_version() == f_major)
      {
        if (get_minor_version() >= f_minor)
          goto end;
      }
    }

    call_notice("Outdated version! Your version: " + get_version_string()
                + "\nPlease go to aceattorneyonline.com to update.");
    destruct_courtroom();
    destruct_lobby();
  }
  else if (header == "checkconnection")
  {
    send_server_packet(new AOPacket("CH#" + QString::number(w_courtroom->get_cid()) + "#%"));
  }

  end:

  delete p_packet;
}

void AOApplication::send_ms_packet(AOPacket *p_packet)
{
  p_packet->net_encode();

  QString f_packet = p_packet->to_string();

  net_manager->ship_ms_packet(f_packet);

  qDebug() << "S(ms):" << f_packet;

  delete p_packet;
}

void AOApplication::send_server_packet(AOPacket *p_packet, bool encoded)
{
  if (encoded)
    p_packet->net_encode();

  QString f_packet = p_packet->to_string();

  if (encryption_needed)
  {
    qDebug() << "S(e):" << f_packet;

    p_packet->encrypt_header(s_decryptor);
    f_packet = p_packet->to_string();
  }
  else
  {
    qDebug() << "S:" << f_packet;
  }

  net_manager->ship_server_packet(f_packet);

  delete p_packet;
}
