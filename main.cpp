#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/array.hpp>
//#include <boost/thread.hpp>
//#include <boost/thread/thread.hpp>


#include <memory>
#include <thread>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <iostream>
using namespace boost;
using namespace boost::asio;
using namespace std::chrono;
using namespace std;
using ip::tcp;

std::mutex mut; // Мьютекс для синхронизации
struct talk_to_client;
extern std::vector <talk_to_client *> clients; // Описание вектора указателей на клиентов

struct talk_to_client // Класс для взаимодействия с клиентом
{
	talk_to_client(asio::io_service &ios) {  // ... 
		sock_ = new ip::tcp::socket(ios); // Создание сокета для клиента
		timeout = false; // Клиент активен
	}
//	std::string username() const { return username_; }
	
	
	ip::tcp::socket & sock() { return *sock_; } // Возвращаем ссылку на сокет
	bool timed_out() const
	{
		return timeout;
	}
	void stop() // Завершение работы с клиентом
	{
		// Завершение приема передачи, закрытие сокета
		// Оставка и закрытие сокета
		sock_->shutdown(asio::socket_base::shutdown_send);
		boost::system::error_code err; sock_->close(err);
	}

	void writeToSocket(std::string& buf) {  // Отправка строки клиенту (запись в сокет)
		std::size_t total_bytes_written = 0;
		while (total_bytes_written != buf.length()) {
			total_bytes_written += sock_->write_some(
				asio::buffer(buf.c_str() +
					total_bytes_written,
					buf.length() - total_bytes_written));
		}
	}

	bool flagFirst = true; // Флаг первого подключения
	void readToWrite() // Метод получения запроса от клиента и ответа клиенту
	{
		// cout << endl << "Vhod In readToWrite";
		if (flagFirst) cout << endl << "Client connected!\n";
		boost::array<char, 256> buf; // Буфер для приема сообщения от клиента
		boost::system::error_code error; // Суда записываем код ошибки (не используем)
		size_t len = sock_->read_some(boost::asio::buffer(buf), error); // Читаем данные от клиента
		buf[len] = 0; // Строка заканчивается 0
		bool flagNotClient = false; // Нет данных от клиента
		if (buf.data() == nullptr) flagNotClient = true;// Данных от клиента нет
		if (!flagNotClient) if (strcmp(buf.data(), "") == 0) flagNotClient = true; // Данных от клиента нет
		if (flagNotClient)  // Данных от клиента нет
		{
			system_clock::time_point end = std::chrono::system_clock::now();   // Полуем текущее время
			if (std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count() >= 5000) { // Если клиент не подключался более 5 сек
				// Действия по отключения клиента
				stop();
				delete sock_;
				timeout = true; // Клиент не активен
			}
			return; // Выйдем из фнкции, когда данных от клиента нет
		}
	    // Данные от клиент пришли
		now = std::chrono::system_clock::now(); // Время получения данных
		cout << "From client: " << buf.data(); // Печать данных от клиента
		// Отправляем данные клиенту
		string strToClient="";
		if (flagFirst)  // При первом подключении
		{
			username_ = buf.data(); // Сохраем имя клиента
			strToClient = "login_ok\n";  // Ответ клиенты
			flagFirst = false; // Следующее подключение уже не первое
		}
		else { // при не первом подключении
			string fromClient = buf.data(); // Данные от клиент в string
			if (fromClient == "ping\n") strToClient = "ping_ok\n";// От клиента пришло ping
			else
				if (fromClient == "clients\n") // От клиента пришло clients
				{
				// Записываем в строку имена всех подключенных клиентов
				for (auto pos : clients)
					strToClient += pos->username_ + " ";
				strToClient += "\n";

				}
				else strToClient = "Unknown format\n"; // Ответ на все неизвестные запросы
			
		}
		cout << "To client: " << strToClient; // Печаем то что отправляется клиенту
		writeToSocket(strToClient);  // Отправка ответа клиенту

	}
	

private:
	// ... same as in Synchronous Client
	
	ip::tcp::socket *sock_; // Указатель на сокет сервера
	bool timeout; // Определяет активность (неактивность) клиента, клиент неактивен, если нет запроса в течение 5 секунд
	std::string username_; // Имя клиента
	system_clock::time_point now; // Время последнего подключения клиента
	

};

std::vector <talk_to_client *> clients; // Вектор указателей на объеты класса для работы с клиентами

bool predicatTimeOut(talk_to_client *pCl)
{
	return pCl->timed_out();
}

void accept_thread() { // Потоковая функция для подключения клиентов
	asio::io_service ios;
	// Создаем слушатель - точку соединения
	ip::tcp::acceptor acceptor(ios, ip::tcp::endpoint(ip::tcp::v4(), 3333));
	while (true) { // Цикл ожидаем подключения клиентов
		talk_to_client * client = new talk_to_client(ios); // talk_to_client -  class для обслуживания клиент
		// cout << "Ожидаем подключения!!!!" << endl;
		acceptor.accept(client->sock()); // Здесь ожидаем подключения!!!
		// Попадаем сюда, когда клиент подключился
		std::lock_guard <std::mutex> lock(mut);  // Блокировка для синхронизации потоков ниже код выполняется только в одном потоке
		clients.push_back(client); // Объект для работы с клиентом добавляем в вектор
	}
}

void handle_clients_thread() { //  Потоковая функция для обслуживания подключенных клиентов
	while (true) {
	    // Проверка клиентов каждую 1 мс
		std::this_thread::sleep_for(std::chrono::milliseconds(1));// Задерка 1 мс
		std::lock_guard <std::mutex> lock(mut); // Блокировка для синхронизации потоков ниже код выполняется только в одном потоке
		for (auto& client : clients) { // В цикле проверяем клиентов
			client->readToWrite(); // Для каджого клиента вызываем функция чтения данных от клиента и ответа, если есть запрос
		}
		auto posDel=std::remove_if(clients.begin(), clients.end(), predicatTimeOut); // Удаляем неактивных клиентов, активность проверяем функция-предикат predicatTimeOut
		clients.erase(posDel, clients.end()); // Окончательное удаление из памяти неактивных клиентов
		
	}
}
int main(int argc, char* argv[])
{

	thread t1(accept_thread);
	thread t2(handle_clients_thread);
	t1.join();
	t2.join();
}
