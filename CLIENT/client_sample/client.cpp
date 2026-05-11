#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <unordered_map>
using namespace std;

#include "..\..\iocp_game_server\iocp_game_server\protocol_2026.h"

sf::TcpSocket socket;

constexpr auto SCREEN_WIDTH = 16;
constexpr auto SCREEN_HEIGHT = 16;
constexpr auto MINIMAP_SIZE = 200.f;
sf::View minimap_view;

constexpr auto TILE_WIDTH = 65;
constexpr auto WINDOW_WIDTH = SCREEN_WIDTH * TILE_WIDTH;   // size of window
constexpr auto WINDOW_HEIGHT = SCREEN_WIDTH * TILE_WIDTH;
constexpr int BUF_SIZE = 1024;

int g_left_x;
int g_top_y;
int g_myid;

sf::RenderWindow* g_window;
sf::Font *g_font;

class OBJECT {
private:
	bool m_showing;
	sf::Sprite m_sprite;
	sf::Text m_name;
public:
	int m_x, m_y;
	char name[MAX_NAME_LEN];
	OBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
	}
	OBJECT() {
		m_showing = false;
	}
	void show()
	{
		m_showing = true;
	}
	void hide()
	{
		m_showing = false;
	}

	void a_move(int x, int y) {
		m_sprite.setPosition((float)x, (float)y);
	}

	void a_draw() {
		g_window->draw(m_sprite);
	}

	void move(int x, int y) {
		m_x = x;
		m_y = y;
	}
	void draw() {
		if (false == m_showing) return;
		float rx = (m_x - g_left_x) * 65.0f + 1;
		float ry = (m_y - g_top_y) * 65.0f + 1;
		m_sprite.setPosition(rx, ry);
		g_window->draw(m_sprite);
		auto size = m_name.getGlobalBounds();
		m_name.setPosition(rx + 32 - size.width / 2, ry - 10);
		g_window->draw(m_name);
	}

	void set_name(const char str[]) {
		m_name.setFont(*g_font);
		m_name.setString(str);
		m_name.setCharacterSize(20);
		m_name.setFillColor(sf::Color(255, 255, 0));
		m_name.setStyle(sf::Text::Bold);
	}
};

OBJECT avatar;
std::unordered_map<int, OBJECT> players;
std::unordered_map<int, OBJECT> npcs;

OBJECT white_tile;
OBJECT black_tile;
std::string avatar_name;

sf::Texture* board;
sf::Texture* pieces;

void client_initialize()
{
	board = new sf::Texture;
	pieces = new sf::Texture;
	board->loadFromFile("chessmap.bmp");
	pieces->loadFromFile("chess2.png");
	g_font = new sf::Font;
	if (false == g_font->loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		exit(-1);
	}
	white_tile = OBJECT{ *board, 5, 5, TILE_WIDTH, TILE_WIDTH };
	black_tile = OBJECT{ *board, 69, 5, TILE_WIDTH, TILE_WIDTH };
	// chess2.png 순서: 룩(0), 비숍(64), 퀸(128), 킹(192), 폰(256), 나이트(320)
	avatar = OBJECT{ *pieces, 192, 0, 64, 64 };  // 킹
	avatar.set_name(avatar_name.c_str());
	avatar.move(4, 4);
	minimap_view.setViewport(sf::FloatRect(0.75f, 0.05f, 0.2f, 0.2f));
}

void client_finish()
{
	players.clear();
	npcs.clear();
	delete g_font;
	delete board;
	delete pieces;
}

void send_packet(void* packet)
{
	unsigned char* p = reinterpret_cast<unsigned char*>(packet);
	size_t sent = 0;
	socket.send(packet, p[0], sent);
}

void ProcessPacket(char* ptr)
{
	static bool first_time = true;
	switch (ptr[1])
	{
	case S2C_LOGIN_RESULT:
	{
		S2C_LoginResult* packet = reinterpret_cast<S2C_LoginResult*>(ptr);
		if (packet->success) {
			std::cout << "Login Success! : " << packet->message << std::endl;
			C2S_Login p;
			p.size = sizeof(p);
			p.type = C2S_LOGIN;
			strcpy_s(p.username, avatar_name.c_str());
			send_packet(&p);
		}
		else {
			std::cout << "Login Failed! : " << packet->message << std::endl;
			socket.disconnect();
		}
		break;
	}
	case S2C_AVATAR_INFO:
	{
		S2C_AvatarInfo* packet = reinterpret_cast<S2C_AvatarInfo*>(ptr);
		g_myid = packet->playerId;
		avatar.m_x = packet->x;
		avatar.m_y = packet->y;
		g_left_x = packet->x - SCREEN_WIDTH / 2;
		g_top_y = packet->y - SCREEN_HEIGHT / 2;
		avatar.show();
		break;
	}

	case S2C_ADD_PLAYER:
	{
		S2C_AddPlayer* my_packet = reinterpret_cast<S2C_AddPlayer*>(ptr);
		int id = my_packet->playerId;
		if (id >= NPC_ID_START) {
			npcs[id] = OBJECT{ *pieces, 256, 0, 64, 64 };  // 폰
			npcs[id].move(my_packet->x, my_packet->y);
			npcs[id].set_name(my_packet->username);
			npcs[id].show();
		} else {
			players[id] = OBJECT{ *pieces, 0, 0, 64, 64 };  // 룩
			players[id].move(my_packet->x, my_packet->y);
			players[id].set_name(my_packet->username);
			players[id].show();
		}
		break;
	}
	case S2C_MOVE_PLAYER:
	{
		S2C_MovePlayer* my_packet = reinterpret_cast<S2C_MovePlayer*>(ptr);
		int other_id = my_packet->playerId;
		if (other_id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->y - SCREEN_HEIGHT / 2;
		} else if (npcs.count(other_id)) {
			npcs[other_id].move(my_packet->x, my_packet->y);
		} else {
			players[other_id].move(my_packet->x, my_packet->y);
		}
		break;
	}

	case S2C_REMOVE_PLAYER:
	{
		S2C_RemovePlayer* my_packet = reinterpret_cast<S2C_RemovePlayer*>(ptr);
		int other_id = my_packet->playerId;
		if (other_id == g_myid)
			avatar.hide();
		else if (npcs.count(other_id))
			npcs.erase(other_id);
		else
			players.erase(other_id);
		break;
	}
	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}
}

void process_data(char* net_buf, size_t io_byte)
{
	char* ptr = net_buf;
	static size_t in_packet_size = 0;
	static size_t saved_packet_size = 0;
	static char packet_buffer[BUF_SIZE];

	while (0 != io_byte) {
		if (0 == in_packet_size) in_packet_size = ptr[0];
		if (io_byte + saved_packet_size >= in_packet_size) {
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			io_byte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		}
		else {
			memcpy(packet_buffer + saved_packet_size, ptr, io_byte);
			saved_packet_size += io_byte;
			io_byte = 0;
		}
	}
}

void client_main()
{
    char net_buf[BUF_SIZE];
    size_t received;
    auto recv_result = socket.receive(net_buf, BUF_SIZE, received);
    if (recv_result != sf::Socket::NotReady && received > 0) process_data(net_buf, received);

    sf::View main_view = g_window->getDefaultView();
    g_window->setView(main_view);

    for (int i = 0; i < SCREEN_WIDTH; ++i) {
        for (int j = 0; j < SCREEN_HEIGHT; ++j) {
            int tile_x = i + g_left_x;
            int tile_y = j + g_top_y;
            if ((tile_x < 0) || (tile_y < 0)) continue;
            if (0 == (tile_x / 3 + tile_y / 3) % 2) {
                white_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
                white_tile.a_draw();
            } else {
                black_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
                black_tile.a_draw();
            }
        }
    }
    
    avatar.draw();
    for (auto& pl : players) pl.second.draw();
    for (auto& npc : npcs) npc.second.draw();

    sf::Text text;
    text.setFont(*g_font);
    char buf[100];
    sprintf_s(buf, "(%d, %d)", avatar.m_x, avatar.m_y);
    text.setString(buf);
    g_window->draw(text);

	// 미니맵: 플레이어 주변 5x5 섹터 (섹터 크기=9타일, 총 45x45 타일 범위)
	constexpr float HALF = 2.5f * 9.f;  // 22.5 타일
	float view_left = (float)avatar.m_x - HALF;
	float view_top  = (float)avatar.m_y - HALF;
	minimap_view.reset(sf::FloatRect(view_left, view_top, HALF * 2.f, HALF * 2.f));
	g_window->setView(minimap_view);

	sf::RectangleShape minimap_bg(sf::Vector2f(HALF * 2.f, HALF * 2.f));
	minimap_bg.setPosition(view_left, view_top);
	minimap_bg.setFillColor(sf::Color(0, 0, 0, 200));
	minimap_bg.setOutlineThickness(1.f);
	minimap_bg.setOutlineColor(sf::Color::White);
	g_window->draw(minimap_bg);

	// NPC 점: 초록 (작게)
	sf::CircleShape npc_dot(0.8f);
	npc_dot.setFillColor(sf::Color::Green);
	for (auto& npc : npcs) {
		npc_dot.setPosition((float)npc.second.m_x, (float)npc.second.m_y);
		g_window->draw(npc_dot);
	}

	// 다른 플레이어 점: 빨강
	sf::CircleShape other_dot(1.5f);
	other_dot.setFillColor(sf::Color::Red);
	for (auto& pl : players) {
		other_dot.setPosition((float)pl.second.m_x, (float)pl.second.m_y);
		g_window->draw(other_dot);
	}

	// 내 캐릭터 점: 노랑 (가장 크게)
	sf::CircleShape my_dot(2.0f);
	my_dot.setFillColor(sf::Color::Yellow);
	my_dot.setPosition((float)avatar.m_x, (float)avatar.m_y);
	g_window->draw(my_dot);

	g_window->setView(g_window->getDefaultView());
}

int main()
{
	wcout.imbue(locale("korean"));
	std::string server_ip;
	std::cout << "Enter Server IP (default: 127.0.0.1): ";
	std::getline(std::cin, server_ip);
	if (server_ip.empty()) server_ip = "127.0.0.1";
	std::cout << "Enter User Name : ";
	std::cin >> avatar_name;
	sf::Socket::Status status = socket.connect(server_ip, PORT);
	socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		wcout << L"������ ������ �� �����ϴ�.\n";
		while (true);
	}

	client_initialize();

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
			if (event.type == sf::Event::KeyPressed) {
				DIRECTION direction;;
				switch (event.key.code) {
				case sf::Keyboard::Left:
					direction = LEFT;
					break;
				case sf::Keyboard::Right:
					direction = RIGHT;
					break;
				case sf::Keyboard::Up:
					direction = UP;
					break;
				case sf::Keyboard::Down:
					direction = DOWN;
					break;
				case sf::Keyboard::Escape:
					window.close();
					break;
				}
				if (-1 != direction) {
					C2S_Move p;
					p.size = sizeof(p);
					p.type = C2S_MOVE;
					p.dir = direction;
					send_packet(&p);
				}

			}
		}

		window.clear();
		client_main();
		window.display();
	}
	client_finish();

	return 0;
}