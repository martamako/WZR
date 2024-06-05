#define _OBJECTS__H
#include <stdio.h>
#include "quaternion.h"

#define PI 3.1416
extern class Terrain;

struct ObjectState
{
	int iID;                  // identyfikator obiektu
	Vector3 vPos;             // position obiektu (�rodka geometrycznego obiektu) 
	quaternion qOrient;       // orientacja (position katowe)
	Vector3 vV, vA;            // predkosc, przyspiesznie liniowe
	Vector3 vV_angular, vA_angular;   // predkosc i przyspeszenie liniowe
	float wheel_turn_angle;     // front wheels rotation angle  (positive - to the left)

	float mass_total;          // masa ca�kowita (wa�na przy kolizjach)
	long money;
	float amount_of_fuel;
	int iID_owner;
	int if_autonomous;
};


class MovableObject
{
public:
	int iID;                  // identyfikator obiektu

	ObjectState state;

	// parametry akcji:
	float F;                  // F - si�a pchajaca do przodu (do ty�u ujemna)
	float breaking_degree;                // stopie� hamowania Fh_max = friction_ground*Fy*breaking_degree
	//float wheel_turn_angle;               // kat skretu kol w radianach (w lewo - dodatni)
	float wheel_turn_speed;      // steering wheel speed [rad/s]
	bool if_keep_steer_wheel;    // czy kierownica zablokowana (je�li nie, to wraca do po�o�enia standardowego)

	// parametry sta�e:
	float planting_skills,    // umiej�tno�� sadzenia drzew (1-pe�na, 0- brak)
		fuel_collection_skills,   // umiej�tno�� zbierania paliwa
		money_collection_skills;

	bool if_selected;            // czy obiekt zaznaczony  

	// parametry fizyczne pojazdu:
	float F_max;
	float alpha_max;
	float Fy;                 // si�a nacisku na podstaw� pojazdu - gdy obiekt styka si� z pod�o�em (od niej zale�y si�a hamowania)
	float mass_own;		  // masa w�asna obiektu (bez paliwa)	
	//float mass_total;          // masa ca�kowita (wa�na przy kolizjach)
	float length, width, height; // rozmiary w kierunku lokalnych osi x,y,z
	float radius;            // radius minimalnej sfery, w ktora wpisany jest obiekt
	float clearance;           // wysoko�� na kt�rej znajduje si� podstawa obiektu (prze�wit)
	float front_axis_dist;           // odleg�o�� od przedniej osi do przedniego zderzaka 
	float back_axis_dist;             // odleg�o�� od tylniej osi do tylniego zderzaka 
	float wheel_ret_speed;  // steering wheel return speed[rad / s]
	float tank_capacity;    // amount of fuel which can be tanked 

	// inne: 
	int iID_collider;            // identyfikator pojazdu z kt�rym nast�pi�a kolizja (-1  brak kolizji)
	Vector3 vdV_collision;        // poprawka pr�dko�ci pojazdu koliduj�cego
	//int iID_owner;
	//bool if_autonomous;         // czy obiekt jest autonomiczny
	long number_of_taking_item;   // numer wzietego przedmiotu
	float taking_value;   // value wzietego przedmiotu
	long number_of_renewed_item;


	float time_of_simulation;     // czas sumaryczny symulacji obiektu   
	Terrain *terrain;             // wska�nik do terrainu, do kt�rego przypisany jest obiekt

public:
	MovableObject(Terrain *t);          // konstruktor
	~MovableObject();
	void ChangeState(ObjectState __state);          // zmiana stanu obiektu
	ObjectState State();        // metoda zwracajaca state obiektu
	void Simulation(float dt);  // symulacja ruchu obiektu w oparciu o biezacy state, przylozone sily
	// oraz czas dzialania sil. Efektem symulacji jest nowy state obiektu 
	void DrawObject();			   // odrysowanie obiektu					
};

enum ItemTypes { ITEM_COIN, ITEM_BARREL, ITEM_TREE, ITEM_BUILDING, ITEM_POINT, ITEM_EDGE, ITEM_START_PLACE, ITEM_WALL, ITEM_GATE };
//char *PRZ_nazwy[] = { "moneta", "beczka", "drzewo", "punkt", "krawedz", "miejsce startowe", "mur", "bramka" };
enum TreeSubtypes { TREE_POPLAR, TREE_SPRUCE, TREE_BAOBAB, TREE_FANTAZJA};
//char *DRZ_nazwy[] = { "topola", "swierk", "baobab", "fantazja" };
enum PointSubtypes {POINT_ORDINAL, POINT_OF_EDGE, POINT_OF_WATER, POINT_OF_VEHICLE};

struct Item
{
	Vector3 vPos;                  // po�o�enie �rodka obiektu
	quaternion qOrient;            // orientacja (po�o�enie katowe, praktycznie nieu�ywane)

	int type;
	int subtype;
	long index;                    // identyfikator - numer przedmiotu (jest potrzebny przy przesy�aniu danych z tego wzgl�du,
	//    �e wyszukiwanie przedmiot�w (np. ItemsInRadius()) zwraca wska�niki)

	float value;                   // w zal. od typu nomina� monety /ilosc paliwa, itd.
	float diameter;                // np. grubosc pnia u podstawy, diameter monety
	float diameter_visual;         // �rednica widoczno�ci - sfery, kt�ra opisuje przedmiot
	float param_f[3];              // dodatkowe parametry roznych typow
	long param_i[3];
	bool if_selected;              // czy przedmiot jest zaznaczony przez uzytkownika
	long group;                    // numer grupy do kt�rej nale�y przedmiot;

	bool to_take;                  // czy przedmiot mozna wziac
	bool if_taken_by_me;           // czy przedmiot wziety przeze mnie
	bool if_renewable;             // czy mo�e si� odnawia� w tym samym miejscu po pewnym czasie
	long taking_time;              // czas wzi�cia (potrzebny do przywr�cenia)

	
	unsigned long display_number;  // zgodny z liczba wy�wietle� po to, by nie powtarza� rysowania przedmiot�w
};

struct LassoPoint
{
	float x;
	float y;
};

class Sector
{
public:
	long w, k;                    // wiersz i kolumna okre�laj�ce po�o�enie obszaru na nieograniczonej p�aszczy�nie
	int number_of_cells;             // liczba oczek na mapie w poziomie i pionie (pot�ga dw�jki!) 
	//float sector_size;        // potrzebny do obliczenia normalnych
	Item **wp;               // jednowymiarowa tablica wska�nik�w do przedmiot�w znajduj�cych si� w pewnym obszarze
	long number_of_items;      // liczba przedmiot�w w obszarze 
	long number_of_items_max;  // size tablicy przedmiot�w z przydzielon� pami�ci�

	float **map_of_heights;         // wysoko�ci wierzcho�k�w
	int ***type_of_surface;                 // typy nawierzchni w ka�dym tr�jk�cie w ka�dym oczku 
	float ***level_of_water;            // poziom wody w ka�dym tr�jk�cie w ka�dym oczku 
	Vector3 ****normal_vectors;                // wektory normalne do poszczeg�lnych p�aszczyzn (rozdzielczo�� x wiersz x kolumna x 4 p�aszczyzny);

	float **map_of_heights_in_edit;    // mapa w trakcie edycji - powinna by� widoczna ��cznie z dotychczasow� map�
	Vector3 ****normal_vectors_in_edit;   
	int ***type_of_surface_in_edit;
	float ***level_of_water_in_edit;
	int number_of_cells_in_edit;

	// podstawowe w�a�ciwo�ci dla ca�ego sektora, gdyby nie by�o mapy
	int default_type_of_surface;                              // type nawierzchni gdyby nie by�o mapki
	float default_height;                                     // std. poziom gruntu (praktycznie nie u�ywany)
	float default_level_of_water;                             // (praktycznie nie u�ywany)

	MovableObject **mov_obj;                          // tablica obiekt�w ruchomych, kt�re mog� znale�� si� w obszarze w ci�gu
	// kroku czasowego symulacji (np. obiekty b. szybkie mog� znale�� si� w wielu obszarach)
	long number_of_movable_objects;
	long number_of_movable_objects_max;

	float height_max;             // maksymalna wysoko�� terrainu w sektorze -> istotna dla wyznaczenia rozdzielczo�ci np. przy widoku z g�ry
	int number_of_cells_displayed;   // liczba oczek (rozdzielczo��) aktualnie wy�wietlana, w zal. od niej mo�na dopasowa� s�siednie sectors
	                                // by nie by�o dziur
	int number_of_cells_displayed_prev;  // poprzednia liczba oczek, by uzyska� pa�ne dopasowanie rozdzielczo�ci s�siaduj�cych sektor�w 
	//float radius;                // radius sektora zwykle d�ugo�� po�owy przek�tnej kwadratu, mo�e by� wi�kszy, gdy w
	                              // �rodku du�e r�nice wysoko�ci lub du�e przedmioty lub obiekty

	Sector(){};
	Sector(int _numcells, long _w, long _k, bool if_map);
	~Sector();
	void memory_for_map(int __number_of_cells, bool if_edit, bool if_map);
	void memory_for_water(bool if_edit);
	void memory_for_surf(bool if_edit);
	void memory_release(bool if_edit = 0);
	void insert_item(Item *p);
	void remove_item(Item *p);
	void insert_movable_object(MovableObject *o);
	void release_movable_object(MovableObject *o);
	void calculate_normal_vectors(float sector_size, bool if_edit=0);         // obliczenie wektor�w normalnych N do p�aszczyzn tr�jk�t�w, by nie robi� tego ka�dorazowo przy odrysowywaniu   
};

struct HashTableCell  // kom�rka tablicy rozproszonej (te� mog�aby by� tablic� rozproszon�)
{
	Sector **sectors;  // tablica wska�nik�w do sektor�w (mog� zdarza� si� konflikty -> w jednej kom�rce wiele sektor�w) 
	long number_of_sectors; // liczba sektor�w w kom�rce
	long number_of_sectors_max; // liczba sektor�w, dla kt�rej zarezerwowano pami��
};

class SectorsHashTable      // tablica rozproszona przechowuj�ca sectors
{
	//private:
public:
	long number_of_cells;                                 // liczba kom�rek tablicy
	long general_number_of_sectors;
	long w_min, w_max, k_min, k_max;                     // minimalne i maksymalne wsp�rz�dne sektor�w (przyspieszaj� wyszukiwanie)  
	HashTableCell *cells;
	unsigned long create_key(long w, long k);          // wyznaczanie indeksu kom�rki

public:

	SectorsHashTable();        // na wej�ciu konstruktora liczba kom�rek tablicy
	~SectorsHashTable();

	Sector *find(long w, long k);       // znajdowanie sektora (zwraca NULL je�li nie znaleziono)
	Sector *insert(Sector *s);              // wstawianie sektora do tablicy
	void remove(Sector *s);              // usuwanie sektora
};



struct FoldParams
{
	float fold_diameter;
	float fold_height;
	int fold_number_of_cells;           // liczba oczek w ka�dym sektorze, w kt�rym tworzona jest fa�da 
	bool if_convex;                 // 1-wypuk�o�� w g�r�, 0 - w d�
	float height_variance;        // losowa wariancja wysoko�ci ka�dego wierzcho�ka fa�dy
	int section_shape;
	float internal_diameter;        // pozwala na uzyskanie p�askiego miejsca na g�rze lub dole fa�dy w zal. od wypuk�o�ci
	bool if_constant_descent;        // czy stoki wa�u maj� sta�e pochylenie -> w�wczas maj� one sta�y k�t nachylenia, niezale�nie
	// od wysoko�ci (np. jak nasyp drogowy) -> nie jest brana pod uwag� �rednica fa�dy ale k�t nachylenia
	float descent_angle;             // k�t nachylenia stoku w radianach np. pi/4 == 45 stopni

	int fold_with_terrain_join_type;        // spos�b ��czenia nowoutworzonej fa�dy z obecn� powierzchni� terrainu 
};

class Terrain
{
public:
	void InsertItemIntoSectors(Item *prz);
	void DeleteItemFromSectors(Item *prz);
public:

	long number_of_items;      // liczba przedmiot�w na planszy
	long number_of_items_max;  // size tablicy przedmiot�w

	SectorsHashTable *ts;

	
	// inne istotne w�a�ciwo�ci terrainu:
	float sector_size;        // w [m] zamiast rozmiaru pola
	float time_of_item_renewing;     // czas w [s] po kt�rym nast�puje odnowienie si� przedmiotu 
	bool if_toroidal_world;        // inaczej cyklicznosc -> po dojsciu do granicy przeskakujemy na
	                              // pocz�tek terrainu, zar�wno w pionie, jak i w poziomie (gdy nie ma granicy, terrain nie mo�e by� cykliczny!)
	float border_x, border_z;   // granica terrainu (je�li -1 - terrain niesko�czenie wielki)

	float height_std;            // standardowa wysoko�� terenu (na razie nie mo�na jej zmienia�)
	float level_of_water_std;    // standardowy poziom wody (-1e10 oznacza brak wody) 
	

	// parametry wy�wietlania:
	float detail_level;                 // stopie� szczeg�owo�ci w przedziale 0,1 
	unsigned long number_of_displays;   // potrzebne do unikania podw�jnego rysowania przedmiot�w
	long *selected_items;     // tablica numer�w zaznaczonych przedmiot�w (dodatkowo ka�dy przedmiot posiada pole z informacj� o zaznaczeniu)
	long number_of_selected_items,      // liczba aktualnie zaznaczonych przedmiot�w
		number_of_selected_items_max;   // liczba miejsc w tablicy 

	FoldParams pf_rej[10];        // parametry edycji fa�d (s� tu tylko ze wzgl�du na konieczno�� zapisu do pliku

	Item *p;          // tablica przedmiotow roznego typu

	Terrain();
	~Terrain();
	float GroundHeight(float x, float z);      // okre�lanie wysoko�ci gruntu dla punktu o wsp. (x,z)
	float ItemPointHeight(Vector3 pol, Item *prz); // wysoko�� na jakiej znajdzie si� przeci�cie pionowej linii przech. przez pol na g�rnej powierzchni przedmiotu
	float GeneralHeight(Vector3 pol);                 // okre�lenie wysoko�ci nad najbli�szym przedmiotem lub gruntem patrz�c w d� od punktu pol
	void SectorCoordinates(long *w, long *k, float x, float z);  // na podstawie wsp. po�o�enia punktu (x,z) zwraca wsp. sektora 
	void SectorBeginPosition(float *x, float *z, long w, long k); // na podstawie wsp�rz�dnych sektora (w,k) zwraca po�o�enie punktu pocz�tkowego
	void InsertWater(float x, float z, float poziom_maks);        // wstawianie wody w oczko (funkcja rekurencyjna)
	float LevelOfWater(float x, float z);                         // poziom wody w podanym punkcie terenu 
	float HighestSelectedItemHeight(Vector3 pol);
	Vector3 Cursor3D_CoordinatesWithoutParallax(int X, int Y);
	void DrawObject();	      // odrysowywanie terrainu 
	void GraphicsInitialization();               // tworzenie listy wy�wietlania
	int SaveMapToFile(char filename[]);        // SaveMapToFile mapy w pliku o podanej nazwie
	int OpenMapFromFile(char filename[]);       // OpenMapFromFile mapy z pliku
	void PlaceItemInTerrain(Item *prz);     // m.i. ustalenie wysoko�ci, na kt�rej le�y przedmiot w zal. od ukszta�t. terrainu
	long InsertItemToArrays(Item prz);   // wstawia przedmiot do tablic, zwraca numer przedmiotu w tablicy p[]
	void SelectUnselectItemOrGroup(long nr_prz);// wpisywanie w pole �e jest zaznaczony lub nie, dodawanie/usuwanie do/z listy zaznaczonych 
	void DeleteSelectItems();
	void NewMap();                      // tworzenie nowej mapy (zwolnienie pami�ci dla starej)
	
	long ItemsInRadius(Item*** item_tab_pointer, Vector3 pol, float radius);
	void InsertObjectIntoSectors(MovableObject *ob);
	void DeleteObjectsFromSectors(MovableObject *ob);
	long ObjectsInRadius(MovableObject*** object_tab_pointer, Vector3 pol, float radius);
};