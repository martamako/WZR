﻿/****************************************************
	Wirtualne zespoly robocze - przykladowy projekt w C++
	Do zadań dotyczących współpracy, ekstrapolacji i
	autonomicznych obiektów
	****************************************************/

#include <windows.h>
#include <math.h>
#include <time.h>

#include <gl\gl.h>
#include <gl\glu.h>
#include <iterator> 
#include <map>
using namespace std;

#include "objects.h"
#include "graphics.h"
#include "net.h"


bool if_different_skills = true;          // czy zró¿nicowanie umiejêtnoœci (dla ka¿dego pojazdu losowane s¹ umiejêtnoœci
// zbierania gotówki i paliwa)

FILE* f = fopen("vct_log.txt", "w");     // plik do zapisu informacji testowych

MovableObject* my_vehicle;             // Object przypisany do tej aplikacji

Terrain terrain;
map<int, MovableObject*> network_vehicles;
map<int, MovableObject*> teammates_vehicles;

float fDt;                          // sredni czas pomiedzy dwoma kolejnymi cyklami symulacji i wyswietlania
long VW_cycle_time, counter_of_simulations;     // zmienne pomocnicze potrzebne do obliczania fDt
long start_time = clock();          // czas od poczatku dzialania aplikacji  
long group_existing_time = clock();    // czas od pocz¹tku istnienia grupy roboczej (czas od uruchom. pierwszej aplikacji)      

multicast_net* multi_reciv;         // wsk do obiektu zajmujacego sie odbiorem komunikatow
multicast_net* multi_send;          //   -||-  wysylaniem komunikatow

HANDLE threadReciv;                 // uchwyt w¹tku odbioru komunikatów
extern HWND main_window;
CRITICAL_SECTION m_cs;               // do synchronizacji wątków

bool SHIFT_pressed = 0;
bool CTRL_pressed = 0;
bool ALT_pressed = 0;
bool L_pressed = 0;
//bool rejestracja_uczestnikow = true;   // rejestracja trwa do momentu wziêcia przedmiotu przez któregokolwiek uczestnika,
// w przeciwnym razie trzeba by przesy³aæ ca³y state œrodowiska nowicjuszowi


//negocjacje
bool negotiation = false;
bool other_negotiation = false;
bool offer_ready = false;
int share = 0;
int team_number = 0;

//licytacje
bool auction = false;
bool other_auction = false;
int auction_share = 0;
int iID_auction = 0;
int biggest_bidder = 0;
long auction_time = 0;

// Parametry widoku:
extern ViewParameters par_view;

bool mouse_control = 0;                   // sterowanie pojazdem za pomoc¹ myszki
int cursor_x, cursor_y;                         // polo¿enie kursora myszki w chwili w³¹czenia sterowania

extern float TransferSending(int ID_receiver, int transfer_type, float transfer_value);

enum frame_types {
	OBJECT_STATE, ITEM_TAKING, ITEM_RENEWAL, COLLISION, TRANSFER, NEGOTIATION, NEGOTIATION_STOP, NEGOTIATION_ACCEPT, AUCTION, AUCTION_STOP, AUCTION_ACCEPT, AUCTION_PRZEBICIE
};

enum transfer_types { MONEY, FUEL };

struct Frame
{
	int iID;
	int frame_type;
	ObjectState state;

	int iID_receiver;      // nr ID adresata wiadomoœci (pozostali uczestnicy powinni wiadomoœæ zignorowaæ)
	int iID_auction_owner;

	int item_number;     // nr przedmiotu, który zosta³ wziêty lub odzyskany
	Vector3 vdV_collision;     // wektor prêdkoœci wyjœciowej po kolizji (uczestnik o wskazanym adresie powinien 
	// przyj¹æ t¹ prêdkoœæ)  

	int transfer_type;        // gotówka, paliwo
	float transfer_value;  // iloœæ gotówki lub paliwa 
	int team_number;

	long existing_time;        // czas jaki uplyn¹³ od uruchomienia programu
	long auction_time;  //czas, kiedy ostatnia oferta licytacji się była wysłana

	int share;
};


//******************************************
// Funkcja obs³ugi w¹tku odbioru komunikatów 
DWORD WINAPI ReceiveThreadFunction(void* ptr)
{
	multicast_net* pmt_net = (multicast_net*)ptr;  // wskaŸnik do obiektu klasy multicast_net
	int size;                                 // liczba bajtów ramki otrzymanej z sieci
	Frame frame;
	ObjectState state;

	while (1)
	{
		size = pmt_net->reciv((char*)&frame, sizeof(Frame));   // oczekiwanie na nadejœcie ramki 
		// Lock the Critical section
		EnterCriticalSection(&m_cs);               // wejście na ścieżkę krytyczną - by inne wątki (np. główny) nie współdzielił 

		switch (frame.frame_type)
		{
		case OBJECT_STATE:           // podstawowy typ ramki informuj¹cej o stanie obiektu              
		{
			state = frame.state;
			//fprintf(f,"odebrano state iID = %d, ID dla mojego obiektu = %d\n",state.iID,my_vehicle->iID);
			if ((frame.iID != my_vehicle->iID))          // jeœli to nie mój w³asny Object
			{

				if ((network_vehicles.size() == 0) || (network_vehicles[frame.iID] == NULL))         // nie ma jeszcze takiego obiektu w tablicy -> trzeba go stworzyæ
				{
					MovableObject* ob = new MovableObject(&terrain);
					ob->iID = frame.iID;
					network_vehicles[frame.iID] = ob;
					if (frame.existing_time > group_existing_time) group_existing_time = frame.existing_time;
					ob->ChangeState(state);   // aktualizacja stanu obiektu obcego 
					terrain.InsertObjectIntoSectors(ob);
					// wys³anie nowemu uczestnikowi informacji o wszystkich wziêtych przedmiotach:
					for (long i = 0; i < terrain.number_of_items; i++)
						if ((terrain.p[i].to_take == 0) && (terrain.p[i].if_taken_by_me))
						{
							Frame frame;
							frame.frame_type = ITEM_TAKING;
							frame.item_number = i;
							frame.state = my_vehicle->State();
							frame.iID = my_vehicle->iID;
							int iSIZE = multi_send->send((char*)&frame, sizeof(Frame));
						}

				}
				else if ((network_vehicles.size() > 0) && (network_vehicles[frame.iID] != NULL))
				{
					terrain.DeleteObjectsFromSectors(network_vehicles[frame.iID]);
					network_vehicles[frame.iID]->ChangeState(state);   // aktualizacja stanu obiektu obcego 	
					terrain.InsertObjectIntoSectors(network_vehicles[frame.iID]);
				}

			}
			break;
		}
		case ITEM_TAKING:            // frame informuj¹ca, ¿e ktoœ wzi¹³ przedmiot o podanym numerze
		{
			state = frame.state;
			if ((frame.item_number < terrain.number_of_items) && (frame.iID != my_vehicle->iID))
			{
				terrain.p[frame.item_number].to_take = 0;
				terrain.p[frame.item_number].if_taken_by_me = 0;
			}
			break;
		}
		case ITEM_RENEWAL:       // frame informujaca, ¿e przedmiot wczeœniej wziêty pojawi³ siê znowu w tym samym miejscu
		{
			if (frame.item_number < terrain.number_of_items)
				terrain.p[frame.item_number].to_take = 1;
			break;
		}
		case COLLISION:                       // frame informuj¹ca o tym, ¿e Object uleg³ kolizji
		{
			if (frame.iID_receiver == my_vehicle->iID)  // ID pojazdu, który uczestniczy³ w kolizji zgadza siê z moim ID 
			{
				my_vehicle->vdV_collision = frame.vdV_collision; // przepisuje poprawkê w³asnej prêdkoœci
				my_vehicle->iID_collider = my_vehicle->iID; // ustawiam nr. kolidujacego jako w³asny na znak, ¿e powinienem poprawiæ prêdkoœæ
			}
			break;
		}
		case NEGOTIATION:
		{
			if (frame.iID_receiver != my_vehicle->iID) {
				other_negotiation = true;
				sprintf(par_view.inscription2, "Oferta_wspolpracy_od = %d _z_podzialem = %d", frame.iID_receiver, frame.share);
			}
			break;
		}

		case NEGOTIATION_STOP:
		{
			if (frame.iID_receiver != my_vehicle->iID) {
				other_negotiation = false;
				negotiation = false;
				offer_ready = false;
			}
			break;
		}

		case NEGOTIATION_ACCEPT:
		{
			if (frame.iID_receiver != my_vehicle->iID) {
				share = 100 - frame.share;
			}
			team_number = frame.team_number;

			other_negotiation = false;
			negotiation = false;
			offer_ready = false;
			sprintf(par_view.inscription2, "Dolaczyles_do_druzyny = %d", frame.team_number);
			break;
		}

		case AUCTION:
		{
			if (frame.iID != my_vehicle->iID) { // jeżeli inna osoba rozpoczyna aukcję
				other_auction = true;
				auction = false;
				iID_auction = frame.iID;
				auction_share = frame.share;
				auction_time = frame.auction_time;
				sprintf(par_view.inscription2, "Oferta_licytacji_od = %d _z_cena = %d", frame.iID, auction_share);
			}
			break;
		}

		case AUCTION_STOP:
		{
			other_auction = false;
			auction = false;
			iID_auction = 0;
			biggest_bidder = 0;
			break;
		}

		case AUCTION_PRZEBICIE:
		{
			if (frame.iID != my_vehicle->iID) { // inna osoba złożyła ofertę
				auction_share = frame.share;
				biggest_bidder = frame.iID;
				auction_time = frame.auction_time;
				sprintf(par_view.inscription2, "Oferta_licytacji_od = %d _z_cena = %d", biggest_bidder, frame.share);
			}
			break;
		}

		case AUCTION_ACCEPT:
		{
			if (frame.iID_receiver == my_vehicle->iID) { //Adresat ramki jest osobą, która wygrała licytację
				auction_share = frame.share;
				//my_vehicle->state.money += auction_share; // dostanie pieniędzy 

			}

			//Wysyłanie komuś paliwa
			TransferSending(frame.iID, FUEL, 1.0);
			

			other_auction = false;
			auction = false;
			iID_auction = 0;
			biggest_bidder = 0;
			break;
		}

		case TRANSFER:                       // frame informuj¹ca o przelewie pieniê¿nym lub przekazaniu towaru    
		{
			if (frame.iID_receiver == my_vehicle->iID)  // ID pojazdu, ktory otrzymal przelew zgadza siê z moim ID 
			{
				if (frame.transfer_type == MONEY)
					my_vehicle->state.money += frame.transfer_value;
				else if (frame.transfer_type == FUEL)
					my_vehicle->state.amount_of_fuel += frame.transfer_value;

				// nale¿a³oby jeszcze przelew potwierdziæ (w UDP ramki mog¹ byæ gubione!)
			}
			break;
		}

		} // switch po typach ramek
		// Opuszczenie ścieżki krytycznej / Release the Critical section
		LeaveCriticalSection(&m_cs);               // wyjście ze ścieżki krytycznej
	}  // while(1)
	return 1;
}

// *****************************************************************
// ****    Wszystko co trzeba zrobiæ podczas uruchamiania aplikacji
// ****    poza grafik¹   
void InteractionInitialisation()
{
	DWORD dwThreadId;

	my_vehicle = new MovableObject(&terrain);    // tworzenie wlasnego obiektu
	if (if_different_skills == false)
		my_vehicle->planting_skills = my_vehicle->money_collection_skills = my_vehicle->fuel_collection_skills = 1.0;

	VW_cycle_time = clock();             // pomiar aktualnego czasu

	// obiekty sieciowe typu multicast (z podaniem adresu WZR oraz numeru portu)
	multi_reciv = new multicast_net("224.10.120.75", 10002);      // Object do odbioru ramek sieciowych
	multi_send = new multicast_net("224.10.120.75", 10002);       // Object do wysy³ania ramek

	// uruchomienie watku obslugujacego odbior komunikatow
	threadReciv = CreateThread(
		NULL,                        // no security attributes
		0,                           // use default stack size
		ReceiveThreadFunction,       // thread function
		(void*)multi_reciv,         // argument to thread function
		0,                           // use default creation flags
		&dwThreadId);                // returns the thread identifier

}


// *****************************************************************
// ****    Wszystko co trzeba zrobiæ w ka¿dym cyklu dzia³ania 
// ****    aplikacji poza grafik¹ 
void VirtualWorldCycle()
{
	counter_of_simulations++;


	// obliczenie œredniego czasu pomiêdzy dwoma kolejnnymi symulacjami po to, by zachowaæ  fizycznych 
	if (counter_of_simulations % 50 == 0)          // jeœli licznik cykli przekroczy³ pewn¹ wartoœæ, to
	{                                   // nale¿y na nowo obliczyæ œredni czas cyklu fDt
		char text[200];
		long prev_time = VW_cycle_time;
		VW_cycle_time = clock();
		float fFps = (50 * CLOCKS_PER_SEC) / (float)(VW_cycle_time - prev_time);
		if (fFps != 0) fDt = 1.0 / fFps; else fDt = 1;

		sprintf(par_view.inscription1, " %0.0f_fps, fuel = %0.2f, money = %d,", fFps, my_vehicle->state.amount_of_fuel, my_vehicle->state.money);
		if (counter_of_simulations % 500 == 0) sprintf(par_view.inscription2, "");
	}

	terrain.DeleteObjectsFromSectors(my_vehicle);
	my_vehicle->Simulation(fDt);                    // symulacja w³asnego obiektu
	terrain.InsertObjectIntoSectors(my_vehicle);


	if ((my_vehicle->iID_collider > -1) &&             // wykryto kolizjê - wysy³am specjaln¹ ramkê, by poinformowaæ o tym drugiego uczestnika
		(my_vehicle->iID_collider != my_vehicle->iID)) // oczywiœcie wtedy, gdy nie chodzi o mój pojazd
	{
		Frame frame;
		frame.frame_type = COLLISION;
		frame.iID_receiver = my_vehicle->iID_collider;
		frame.vdV_collision = my_vehicle->vdV_collision;
		frame.iID = my_vehicle->iID;
		int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));

		char text[128];
		sprintf(par_view.inscription2, "Kolizja_z_obiektem_o_ID = %d", my_vehicle->iID_collider);
		//SetWindowText(main_window,text);

		my_vehicle->iID_collider = -1;
	}

	if (negotiation) {
		// Wypisywanie komunikatu o negocjacjach
		if (other_negotiation == false) {
			sprintf(par_view.inscription2, "Oferta_wspolpracy = %d", share);
		}

		if (offer_ready) {
			Frame frame;
			frame.frame_type = NEGOTIATION;
			frame.iID_receiver = my_vehicle->iID;
			frame.team_number = 1;
			frame.share = share;
			int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));

			//char text[128];
			//sprintf(par_view.inscription2, "Kolizja_z_obiektem_o_ID = %d", my_vehicle->iID_collider);
		}
	}
	if (auction) {
		long auction_time_now = clock() - auction_time; //czas licytacji
		sprintf(par_view.inscription2, "Oferta_licytacji = %d,_czas_licytacji = %ld", auction_share, auction_time);
		if (biggest_bidder > 0) {
			sprintf(par_view.inscription2, "Oferta_licytacji = %d,_czas_licytacji = %ld,_licytator = %d", auction_share, auction_time, biggest_bidder);
		}
		if (auction_time_now / CLOCKS_PER_SEC > 6) {
			if (biggest_bidder > 0) {
				float money_auction = TransferSending(biggest_bidder, MONEY, auction_share); // Wysłanie pieniędzy


				Frame frame;
				frame.frame_type = AUCTION_ACCEPT;
				frame.iID = my_vehicle->iID;
				frame.iID_receiver = biggest_bidder;
				frame.share = auction_share;
				int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));
			}
			else {
				auction = false;
				Frame frame;
				frame.frame_type = AUCTION_STOP;
				int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));
			}
			
		}
	}

	if (other_auction) { // jeżeli prowadzę aukcję
		long auction_time_now = clock() - auction_time; //czas licytacji
		sprintf(par_view.inscription2, "Oferta_licytacji = %d,_czas_licytacji = %ld", auction_share, auction_time);
		if (biggest_bidder > 0) {
			sprintf(par_view.inscription2, "Oferta_licytacji = %d,_czas_licytacji = %ld,_licytator = %d", auction_share, auction_time, biggest_bidder);
		}
	}







	// wyslanie komunikatu o stanie obiektu przypisanego do aplikacji (my_vehicle):
	Frame frame;
	frame.frame_type = OBJECT_STATE;
	frame.state = my_vehicle->State();         // state w³asnego obiektu 
	frame.iID = my_vehicle->iID;
	frame.existing_time = clock() - start_time;
	int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));



	// wziêcie przedmiotu -> wysy³anie ramki 
	if (my_vehicle->number_of_taking_item > -1)
	{
		Frame frame;
		frame.frame_type = ITEM_TAKING;
		frame.item_number = my_vehicle->number_of_taking_item;
		frame.state = my_vehicle->State();
		frame.iID = my_vehicle->iID;
		int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));

		sprintf(par_view.inscription2, "Wziecie_przedmiotu_o_wartosci_ %f", my_vehicle->taking_value);

		my_vehicle->number_of_taking_item = -1;
		my_vehicle->taking_value = 0;
	}

	// odnawianie siê przedmiotu -> wysy³anie ramki
	if (my_vehicle->number_of_renewed_item > -1)
	{                             // jeœli min¹³ pewnien okres czasu przedmiot mo¿e zostaæ przywrócony
		Frame frame;
		frame.frame_type = ITEM_RENEWAL;
		frame.item_number = my_vehicle->number_of_renewed_item;
		frame.iID = my_vehicle->iID;
		int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));


		my_vehicle->number_of_renewed_item = -1;
	}

}

// *****************************************************************
// ****    Wszystko co trzeba zrobiæ podczas zamykania aplikacji
// ****    poza grafik¹ 
void EndOfInteraction()
{
	fprintf(f, "Koniec interakcji\n");
	fclose(f);
}

// Funkcja wysylajaca ramke z przekazem, zwraca zrealizowan¹ wartoœæ przekazu
float TransferSending(int ID_receiver, int transfer_type, float transfer_value)
{
	Frame frame;
	frame.frame_type = TRANSFER;
	frame.iID_receiver = ID_receiver;
	frame.transfer_type = transfer_type;
	frame.transfer_value = transfer_value;
	frame.iID = my_vehicle->iID;

	// tutaj nale¿a³oby uzyskaæ potwierdzenie przekazu zanim sumy zostan¹ odjête
	if (transfer_type == MONEY)
	{
		if (my_vehicle->state.money < transfer_value)
			frame.transfer_value = my_vehicle->state.money;
		my_vehicle->state.money -= frame.transfer_value;
		sprintf(par_view.inscription2, "Przelew_sumy_ %f _na_rzecz_ID_ %d", transfer_value, ID_receiver);
	}
	else if (transfer_type == FUEL)
	{
		if (my_vehicle->state.amount_of_fuel < transfer_value)
			frame.transfer_value = my_vehicle->state.amount_of_fuel;
		my_vehicle->state.amount_of_fuel -= frame.transfer_value;
		sprintf(par_view.inscription2, "Przekazanie_paliwa_w_ilosci_ %f _na_rzecz_ID_ %d", transfer_value, ID_receiver);
	}

	if (frame.transfer_value > 0)
		int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));

	return frame.transfer_value;
}





//deklaracja funkcji obslugi okna
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);


HWND main_window;                   // uchwyt do okna aplikacji
HDC g_context = NULL;        // uchwyt kontekstu graficznego

bool terrain_edition_mode = 0;

//funkcja Main - dla Windows
int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow)
{
	//Initilize the critical section:
	InitializeCriticalSection(&m_cs);

	MSG system_message;		  //innymi slowy "komunikat"
	WNDCLASS window_class; //klasa głównego okna aplikacji

	static char class_name[] = "Basic";

	//Definiujemy klase głównego okna aplikacji
	//Okreslamy tu wlasciwosci okna, szczegoly wygladu oraz
	//adres funkcji przetwarzajacej komunikaty
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc = WndProc; //adres funkcji realizującej przetwarzanie meldunków 
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = hInstance; //identyfikator procesu przekazany przez MS Windows podczas uruchamiania programu
	window_class.hIcon = 0;
	window_class.hCursor = LoadCursor(0, IDC_ARROW);
	window_class.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
	window_class.lpszMenuName = "Menu";
	window_class.lpszClassName = class_name;

	//teraz rejestrujemy klasę okna głównego
	RegisterClass(&window_class);

	/*tworzymy main_window główne
	main_window będzie miało zmienne rozmiary, listwę z tytułem, menu systemowym
	i przyciskami do zwijania do ikony i rozwijania na cały ekran, po utworzeniu
	będzie widoczne na ekranie */
	main_window = CreateWindow(class_name, "WZR 2023/24, temat 4, wersja c ", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		100, 100, 1400, 820, NULL, NULL, hInstance, NULL);


	ShowWindow(main_window, nCmdShow);

	//odswiezamy zawartosc okna
	UpdateWindow(main_window);



	// GŁÓWNA PĘTLA PROGRAMU

	// pobranie komunikatu z kolejki jeśli funkcja PeekMessage zwraca wartość inną niż FALSE,
	// w przeciwnym wypadku symulacja wirtualnego świata wraz z wizualizacją
	ZeroMemory(&system_message, sizeof(system_message));
	while (system_message.message != WM_QUIT)
	{
		if (PeekMessage(&system_message, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&system_message);
			DispatchMessage(&system_message);
		}
		else
		{
			VirtualWorldCycle();    // Cykl wirtualnego świata
			InvalidateRect(main_window, NULL, FALSE);
		}
	}

	return (int)system_message.wParam;
}

// ************************************************************************
// ****    Obs³uga klawiszy s³u¿¹cych do sterowania obiektami lub
// ****    widokami 
void MessagesHandling(UINT message_type, WPARAM wParam, LPARAM lParam)
{

	int LCONTROL = GetKeyState(VK_LCONTROL);
	int RCONTROL = GetKeyState(VK_RCONTROL);
	int LALT = GetKeyState(VK_LMENU);
	int RALT = GetKeyState(VK_RMENU);


	switch (message_type)
	{

	case WM_LBUTTONDOWN: //reakcja na lewy przycisk myszki
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (mouse_control)
			my_vehicle->F = my_vehicle->F_max;        // si³a pchaj¹ca do przodu

		break;
	}
	case WM_RBUTTONDOWN: //reakcja na prawy przycisk myszki
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		int LSHIFT = GetKeyState(VK_LSHIFT);   // sprawdzenie czy lewy Shift wciśnięty, jeśli tak, to LSHIFT == 1
		int RSHIFT = GetKeyState(VK_RSHIFT);

		if (mouse_control)
			my_vehicle->F = -my_vehicle->F_max / 2;        // si³a pchaj¹ca do tylu
		else if (wParam & MK_SHIFT)                    // odznaczanie wszystkich obiektów   
		{
			for (long i = 0; i < terrain.number_of_selected_items; i++)
				terrain.p[terrain.selected_items[i]].if_selected = 0;
			terrain.number_of_selected_items = 0;
		}
		else                                          // zaznaczenie obiektów
		{
			RECT r;
			//GetWindowRect(main_window,&r);
			GetClientRect(main_window, &r);
			//Vector3 w = Cursor3dCoordinates(x, r.bottom - r.top - y);
			Vector3 w = terrain.Cursor3D_CoordinatesWithoutParallax(x, r.bottom - r.top - y);


			//float radius = (w - point_click).length();
			float min_dist = 1e10;
			long index_min = -1;
			bool if_movable_obj;
			for (map<int, MovableObject*>::iterator it = network_vehicles.begin(); it != network_vehicles.end(); ++it)
			{
				if (it->second)
				{
					MovableObject* ob = it->second;
					float xx, yy, zz;
					ScreenCoordinates(&xx, &yy, &zz, ob->state.vPos);
					yy = r.bottom - r.top - yy;
					float odl_kw = (xx - x) * (xx - x) + (yy - y) * (yy - y);
					if (min_dist > odl_kw)
					{
						min_dist = odl_kw;
						index_min = ob->iID;
						if_movable_obj = 1;
					}
				}
			}


			// trzeba to przerobić na wersję sektorową, gdyż przedmiotów może być dużo!
			// niestety nie jest to proste. 

			//Item **wsk_prz = NULL;
			//long liczba_prz_w_prom = terrain.ItemsInRadius(&wsk_prz, w,100);

			for (long i = 0; i < terrain.number_of_items; i++)
			{
				float xx, yy, zz;
				Vector3 placement;
				if ((terrain.p[i].type == ITEM_EDGE) || (terrain.p[i].type == ITEM_WALL))
				{
					placement = (terrain.p[terrain.p[i].param_i[0]].vPos + terrain.p[terrain.p[i].param_i[1]].vPos) / 2;
				}
				else
					placement = terrain.p[i].vPos;
				ScreenCoordinates(&xx, &yy, &zz, placement);
				yy = r.bottom - r.top - yy;
				float odl_kw = (xx - x) * (xx - x) + (yy - y) * (yy - y);
				if (min_dist > odl_kw)
				{
					min_dist = odl_kw;
					index_min = i;
					if_movable_obj = 0;
				}
			}

			if (index_min > -1)
			{
				//fprintf(f,"zaznaczono przedmiot %d pol = (%f, %f, %f)\n",ind_min,terrain.p[ind_min].vPos.x,terrain.p[ind_min].vPos.y,terrain.p[ind_min].vPos.z);
				//terrain.p[ind_min].if_selected = 1 - terrain.p[ind_min].if_selected;
				if (if_movable_obj)
				{
					network_vehicles[index_min]->if_selected = 1 - network_vehicles[index_min]->if_selected;

					if (network_vehicles[index_min]->if_selected)
						sprintf(par_view.inscription2, "zaznaczono_ obiekt_ID_%d", network_vehicles[index_min]->iID);
				}
				else
				{
					terrain.SelectUnselectItemOrGroup(index_min);
				}
				//char lan[256];
				//sprintf(lan, "klikniêto w przedmiot %d pol = (%f, %f, %f)\n",ind_min,terrain.p[ind_min].vPos.x,terrain.p[ind_min].vPos.y,terrain.p[ind_min].vPos.z);
				//SetWindowText(main_window,lan);
			}
			Vector3 point_click = Cursor3dCoordinates(x, r.bottom - r.top - y);

		}

		break;
	}
	case WM_MBUTTONDOWN: //reakcja na œrodkowy przycisk myszki : uaktywnienie/dezaktywacja sterwania myszkowego
	{
		mouse_control = 1 - mouse_control;
		cursor_x = LOWORD(lParam);
		cursor_y = HIWORD(lParam);
		break;
	}
	case WM_LBUTTONUP: //reakcja na puszczenie lewego przycisku myszki
	{
		if (mouse_control)
			my_vehicle->F = 0.0;        // si³a pchaj¹ca do przodu
		break;
	}
	case WM_RBUTTONUP: //reakcja na puszczenie lewy przycisk myszki
	{
		if (mouse_control)
			my_vehicle->F = 0.0;        // si³a pchaj¹ca do przodu
		break;
	}
	case WM_MOUSEMOVE:
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (mouse_control)
		{
			float wheel_turn_angle = (float)(cursor_x - x) / 20;
			if (wheel_turn_angle > 45) wheel_turn_angle = 45;
			if (wheel_turn_angle < -45) wheel_turn_angle = -45;
			my_vehicle->state.wheel_turn_angle = PI * wheel_turn_angle / 180;
		}
		break;
	}
	case WM_MOUSEWHEEL:     // ruch kó³kiem myszy -> przybli¿anie, oddalanie widoku
	{
		int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);  // dodatni do przodu, ujemny do ty³u
		//fprintf(f,"zDelta = %d\n",zDelta);          // zwykle +-120, jak siê bardzo szybko zakrêci to czasmi wyjdzie +-240
		if (zDelta > 0) {
			if (par_view.distance > 0.5) par_view.distance /= 1.2;
			else par_view.distance = 0;
		}
		else {
			if (par_view.distance > 0) par_view.distance *= 1.2;
			else par_view.distance = 0.5;
		}

		break;
	}
	case WM_KEYDOWN:
	{

		switch (LOWORD(wParam))
		{
		case VK_SHIFT:
		{
			SHIFT_pressed = 1;
			break;
		}
		case VK_CONTROL:
		{
			CTRL_pressed = 1;
			break;
		}
		case VK_MENU:
		{
			ALT_pressed = 1;
			break;
		}

		case VK_SPACE:
		{
			my_vehicle->breaking_degree = 1.0;       // stopieñ hamowania (reszta zale¿y od si³y docisku i wsp. tarcia)
			break;                       // 1.0 to maksymalny stopieñ (np. zablokowanie kó³)
		}
		case VK_UP:
		{
			if (CTRL_pressed && par_view.top_view)
				par_view.shift_to_bottom += par_view.distance / 2;       // przesunięcie widoku z kamery w górę
			else
				my_vehicle->F = my_vehicle->F_max;        // si³a pchaj¹ca do przodu
			break;
		}
		case VK_DOWN:
		{
			if (CTRL_pressed && par_view.top_view)
				par_view.shift_to_bottom -= par_view.distance / 2;       // przesunięcie widoku z kamery w dół 
			else
				my_vehicle->F = -my_vehicle->F_max / 2;        // sila pchajaca do tylu
			break;
		}
		case VK_LEFT:
		{
			if (CTRL_pressed && par_view.top_view)
				par_view.shift_to_right += par_view.distance / 2;
			else
			{
				if (my_vehicle->steer_wheel_speed < 0) {
					my_vehicle->steer_wheel_speed = 0;
					my_vehicle->if_keep_steer_wheel = true;
				}
				else {
					if (SHIFT_pressed) my_vehicle->steer_wheel_speed = 0.5;
					else my_vehicle->steer_wheel_speed = 0.5 / 4;
				}
			}

			break;
		}
		case VK_RIGHT:
		{
			if (CTRL_pressed && par_view.top_view)
				par_view.shift_to_right -= par_view.distance / 2;
			else
			{
				if (my_vehicle->steer_wheel_speed > 0) {
					my_vehicle->steer_wheel_speed = 0;
					my_vehicle->if_keep_steer_wheel = true;
				}
				else {
					if (SHIFT_pressed) my_vehicle->steer_wheel_speed = -0.5;
					else my_vehicle->steer_wheel_speed = -0.5 / 4;
				}
			}
			break;
		}
		case VK_HOME:
		{
			if (CTRL_pressed && par_view.top_view)
				par_view.shift_to_right = par_view.shift_to_bottom = 0;

			break;
		}
		case 'W':   // przybli¿enie widoku
		{
			//initial_camera_position = initial_camera_position - initial_camera_direction*0.3;
			if (par_view.distance > 0.5)par_view.distance /= 1.2;
			else par_view.distance = 0;
			break;
		}
		case 'S':   // distance widoku
		{
			//initial_camera_position = initial_camera_position + initial_camera_direction*0.3; 
			if (par_view.distance > 0) par_view.distance *= 1.2;
			else par_view.distance = 0.5;
			break;
		}
		case 'Q':   // widok z góry
		{
			par_view.top_view = 1 - par_view.top_view;
			if (par_view.top_view)
				SetWindowText(main_window, "Włączono widok z góry!");
			else
				SetWindowText(main_window, "Wyłączono widok z góry.");
			break;
		}
		case 'E':   // obrót kamery ku górze (wzglêdem lokalnej osi z)
		{
			par_view.cam_angle_z += PI * 5 / 180;
			break;
		}
		case 'D':   // obrót kamery ku do³owi (wzglêdem lokalnej osi z)
		{
			par_view.cam_angle_z -= PI * 5 / 180;
			break;
		}
		case 'A':   // w³¹czanie, wy³¹czanie trybu œledzenia obiektu
		{
			par_view.tracking = 1 - par_view.tracking;
			break;
		}
		case 'Z':   // zoom - zmniejszenie k¹ta widzenia
		{
			par_view.zoom /= 1.1;
			RECT rc;
			GetClientRect(main_window, &rc);
			WindowSizeChange(rc.right - rc.left, rc.bottom - rc.top);
			break;
		}
		case 'X':   // zoom - zwiêkszenie k¹ta widzenia
		{
			par_view.zoom *= 1.1;
			RECT rc;
			GetClientRect(main_window, &rc);
			WindowSizeChange(rc.right - rc.left, rc.bottom - rc.top);
			break;
		}

		case 'F':  // przekazanie 10 kg paliwa pojazdom zaznaczonym
		{
			for (map<int, MovableObject*>::iterator it = network_vehicles.begin(); it != network_vehicles.end(); ++it)
			{
				if (it->second)
				{
					MovableObject* ob = it->second;
					if (ob->if_selected)
						float ilosc_p = TransferSending(ob->iID, FUEL, 10);
				}
			}
			break;
		}
		case 'G':  // przekazanie 100 jednostek gotowki pojazdom zaznaczonym
		{
			for (map<int, MovableObject*>::iterator it = network_vehicles.begin(); it != network_vehicles.end(); ++it)
			{
				if (it->second)
				{
					MovableObject* ob = it->second;
					if (ob->if_selected)
						float ilosc_p = TransferSending(ob->iID, MONEY, 100);
				}
			}
			break;
		}
		case 'N':
		{
			auction = false;
			negotiation = true;
			break;
		}


		case 'M':
		{
			negotiation = false;
			offer_ready = false;
			Frame frame;
			frame.frame_type = NEGOTIATION_STOP;
			frame.iID_receiver = my_vehicle->iID;
			frame.share = share;
			int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));
			break;
		}

		case 'R':
		{
			if (!other_auction) { // jeżeli już ktoś nie prowadzi licytacji rozpoczynam licytację
				negotiation = false;
				auction = true;
				iID_auction = my_vehicle->iID;
				auction_share = 1;
				biggest_bidder = 0;

				Frame frame;
				frame.frame_type = AUCTION;
				frame.iID = my_vehicle->iID;
				frame.share = auction_share;
				auction_time = clock();
				frame.auction_time = auction_time;
				
				frame.iID_auction_owner = iID_auction;
				int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));
			}

			break;
		}

		case 'T': // zakończenie licytacji, jeżeli ją zacząłem
		{
			if (auction) {
				auction = false;
				Frame frame;
				frame.frame_type = AUCTION_STOP;
				int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));
			}
			
			break;
		}

		case VK_NUMPAD8:
		{
			if (negotiation) {
				if (share < 100 && offer_ready == false)
					share++;
			}	
			else if (other_auction) {
				auction_share++;
				sprintf(par_view.inscription2, "nowa cena: %d", auction_share);
			}


			break;
		}
		case VK_NUMPAD2:
		{
			if (negotiation) {
				if (share > 0 && offer_ready == false)
					share--;
			}
			else if (other_auction) { // zmniejszenie stanu oferty w licytacji
				if (auction_share > 0)
					auction_share--;
				sprintf(par_view.inscription2, "nowa cena: %d", auction_share);
			}

			break;
		}
		case VK_NUMPAD5:
		{
			if (negotiation) {
				if (offer_ready) offer_ready = false;
				else offer_ready = true;
			}
			else if (other_auction) { // jeżeli ktoś inny prowadzi licytację to przebijamy
				Frame frame;
				frame.frame_type = AUCTION_PRZEBICIE;
				frame.iID = my_vehicle->iID;
				frame.share = auction_share;
				frame.iID_auction_owner = iID_auction;
				auction_time = clock();
				frame.auction_time = auction_time;
				int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));
			}

			break;
		}
		case VK_NUMPAD7:
		{
			if (other_negotiation) {
				Frame frame;
				frame.frame_type = NEGOTIATION_ACCEPT;
				frame.iID_receiver = my_vehicle->iID;
				frame.share = share;
				frame.team_number = 1;
				int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));
			}

			if (auction) { // jeżeli prowadzimy licytację to wyrażamy zgodę na ofertę
				//Wysłanie pieniędzy do największego licytatora
				float money_auction = TransferSending(biggest_bidder, MONEY, auction_share); // Wysłanie pieniędzy
				

				Frame frame;
				frame.frame_type = AUCTION_ACCEPT;
				frame.iID = my_vehicle->iID;
				frame.iID_receiver = biggest_bidder;
				frame.share = auction_share;
				int iRozmiar = multi_send->send((char*)&frame, sizeof(Frame));

			}
			break;
		}

		case 'L':     // rozpoczęcie zaznaczania metodą lasso
			L_pressed = true;
			break;


		} // switch po klawiszach

		break;
	}

	case WM_KEYUP:
	{
		switch (LOWORD(wParam))
		{
		case VK_SHIFT:
		{
			SHIFT_pressed = 0;
			break;
		}
		case VK_CONTROL:
		{
			CTRL_pressed = 0;
			break;
		}
		case VK_MENU:
		{
			ALT_pressed = 0;
			break;
		}
		case 'L':     // zakonczenie zaznaczania metodą lasso
			L_pressed = false;
			break;
		case VK_SPACE:
		{
			my_vehicle->breaking_degree = 0.0;
			break;
		}
		case VK_UP:
		{
			my_vehicle->F = 0.0;

			break;
		}
		case VK_DOWN:
		{
			my_vehicle->F = 0.0;
			break;
		}
		case VK_LEFT:
		{
			if (my_vehicle->if_keep_steer_wheel) my_vehicle->steer_wheel_speed = -0.5 / 4;
			else my_vehicle->steer_wheel_speed = 0;
			my_vehicle->if_keep_steer_wheel = false;
			break;
		}
		case VK_RIGHT:
		{
			if (my_vehicle->if_keep_steer_wheel) my_vehicle->steer_wheel_speed = 0.5 / 4;
			else my_vehicle->steer_wheel_speed = 0;
			my_vehicle->if_keep_steer_wheel = false;
			break;
		}

		}

		break;
	}

	} // switch po komunikatach
}

/********************************************************************
FUNKCJA OKNA realizujaca przetwarzanie meldunków kierowanych do okna aplikacji*/
LRESULT CALLBACK WndProc(HWND main_window, UINT message_type, WPARAM wParam, LPARAM lParam)
{

	// PONIŻSZA INSTRUKCJA DEFINIUJE REAKCJE APLIKACJI NA POSZCZEGÓLNE MELDUNKI 

	MessagesHandling(message_type, wParam, lParam);

	switch (message_type)
	{
	case WM_CREATE:  //system_message wysyłany w momencie tworzenia okna
	{

		g_context = GetDC(main_window);

		srand((unsigned)time(NULL));
		int result = GraphicsInitialization(g_context);
		if (result == 0)
		{
			printf("nie udalo sie otworzyc okna graficznego\n");
			//exit(1);
		}

		InteractionInitialisation();

		SetTimer(main_window, 1, 10, NULL);

		return 0;
	}
	case WM_KEYDOWN:
	{
		switch (LOWORD(wParam))
		{
		case VK_F1:  // wywolanie systemu pomocy
		{
			char lan[1024], lan_bie[1024];
			//GetSystemDirectory(lan_sys,1024);
			GetCurrentDirectory(1024, lan_bie);
			strcpy(lan, "C:\\Program Files\\Internet Explorer\\iexplore ");
			strcat(lan, lan_bie);
			strcat(lan, "\\pomoc.htm");
			int wyni = WinExec(lan, SW_NORMAL);
			if (wyni < 32)  // proba uruchominia pomocy nie powiodla sie
			{
				strcpy(lan, "C:\\Program Files\\Mozilla Firefox\\firefox ");
				strcat(lan, lan_bie);
				strcat(lan, "\\pomoc.htm");
				wyni = WinExec(lan, SW_NORMAL);
				if (wyni < 32)
				{
					char lan_win[1024];
					GetWindowsDirectory(lan_win, 1024);
					strcat(lan_win, "\\notepad pomoc.txt ");
					wyni = WinExec(lan_win, SW_NORMAL);
				}
			}
			break;
		}
		case VK_F4:  // włączanie/ wyłączanie trybu edycji terrainu
		{
			terrain_edition_mode = 1 - terrain_edition_mode;
			if (terrain_edition_mode)
				SetWindowText(main_window, "TRYB EDYCJI TERENU F2-SaveMapToFile, F1-pomoc");
			else
				SetWindowText(main_window, "WYJSCIE Z TRYBU EDYCJI TERENU");
			break;
		}
		case VK_ESCAPE:   // wyjście z programu
		{
			SendMessage(main_window, WM_DESTROY, 0, 0);
			break;
		}
		}
		return 0;
	}

	case WM_PAINT:
	{
		PAINTSTRUCT paint;
		HDC context;
		context = BeginPaint(main_window, &paint);

		DrawScene();
		SwapBuffers(context);

		EndPaint(main_window, &paint);



		return 0;
	}

	case WM_TIMER:

		return 0;

	case WM_SIZE:
	{
		int cx = LOWORD(lParam);
		int cy = HIWORD(lParam);

		WindowSizeChange(cx, cy);

		return 0;
	}

	case WM_DESTROY: //obowiązkowa obsługa meldunku o zamknięciu okna
		if (lParam == 100)
			MessageBox(main_window, "Jest zbyt późno na dołączenie do wirtualnego świata. Trzeba to zrobić zanim inni uczestnicy zmienią jego state.", "Zamknięcie programu", MB_OK);

		EndOfInteraction();
		EndOfGraphics();

		ReleaseDC(main_window, g_context);
		KillTimer(main_window, 1);

		PostQuitMessage(0);
		return 0;

	default: //standardowa obsługa pozostałych meldunków
		return DefWindowProc(main_window, message_type, wParam, lParam);
	}

}
