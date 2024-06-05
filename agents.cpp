#include <stdlib.h>
#include <time.h>
#include "agents.h"
#define PI 3.14159265358979323846

AutoPilot::AutoPilot()
{

}


float alpha_max = PI / 2.0;
int id_wybranego = -1;

void AutoPilot::AutoControl(MovableObject* ob)
{
	// TUTAJ NALE�Y UMIE�CI� ALGORYTM AUTONOMICZNEGO STEROWANIA POJAZDEM
	// .................................................................
	// .................................................................

	Terrain* _terrain = ob->terrain;

	Vector3 vect_local_forward = ob->state.qOrient.rotate_vector(Vector3(1, 0, 0));
	Vector3 vect_local_right = ob->state.qOrient.rotate_vector(Vector3(0, 0, 1));

	bool wybrany_istnieje = false;
	float best_alpha;
	int best_i;

	Vector3 x;
	int side;

	for (int i = 0; i < _terrain->number_of_items; i++)
	{
		if (_terrain->p[i].index == id_wybranego)
		{
			best_i = i;
			wybrany_istnieje = true;
		}
	}

	if (wybrany_istnieje && (_terrain->p[best_i].if_taken_by_me == false))
	{
		x = _terrain->p[best_i].vPos - ob->state.vPos;
		float forward_ratio = vect_local_forward ^ x;
		float alpha = acos(forward_ratio / x.length());
		best_alpha = alpha;

		float right_ratio = vect_local_right ^ x;
		if (right_ratio > 0) {
			side = -1;		//z prawej
		}
		else {
			side = 1;		//z lewej
		}
	}
	else
	{
		bool first = true;

		for (int i = 0; i < _terrain->number_of_items; i++)
		{
			if (_terrain->p[i].to_take == 1)
			{
				Vector3 x = _terrain->p[i].vPos - ob->state.vPos;
				float forward_ratio = vect_local_forward ^ x;
				float alpha = acos(forward_ratio / x.length());

				if (first)
				{
					best_alpha = alpha;
					best_i = i;
					first = false;
					continue;
				}

				if (alpha < best_alpha)
				{
					best_alpha = alpha;
					best_i = i;
				}
			}
		}

		x = _terrain->p[best_i].vPos - ob->state.vPos;
		float right_ratio = vect_local_right ^ x;
		if (right_ratio > 0) {
			side = -1;		//z prawej
		}
		else {
			side = 1;		//z lewej
		}

		id_wybranego = _terrain->p[best_i].index;
	}

	//tutaj


	// Item best_item = _terrain->p[best_i];

	// Item*** lista;
	// long liczba_obiektow = _terrain->ItemsInRadius(lista, ob->state.vPos, 10.0);
	// delete lista;



	// parametry sterowania:
	ob->breaking_degree = 0;						// si�a hamowania
	ob->F = ob->F_max * (1.0 - (best_alpha / alpha_max)) / 2;			// si�a nap�dowa
	ob->wheel_turn_speed = 0;											// pr�dko�� skr�tu kierownicy (dodatnia - w lewo)
	ob->if_keep_steer_wheel = 0;										// czy kierownica zablokowana (je�li nie, to wraca do po�o�enia standardowego)
	ob->state.wheel_turn_angle = side * 45 * (best_alpha / alpha_max);			// k�t skr�tu kierownicy - mo�na ustaiwa� go bezpo�rednio zak�adaj�c, �e robot mo�e kr�ci� kierownic� dowolnie szybko,
	// jednaj gwa�towna zmiana po�o�enia kierownicy (i tym samym k�) mo�e skutkowa� po�lizgiem pojazdu


}

void AutoPilot::ControlTest(MovableObject* _ob, float krok_czasowy, float czas_proby)
{
	bool koniec = false;
	float _czas = 0;               // czas liczony od pocz�tku testu
	//FILE *pl = fopen("test_sterowania.txt","w");
	while (!koniec)
	{
		_ob->Simulation(krok_czasowy);
		AutoControl(_ob);
		_czas += krok_czasowy;
		if (_czas >= czas_proby) koniec = true;
		//fprintf(pl,"czas %f, vPos[%f %f %f], got %d, pal %f, F %f, wheel_turn_angle %f, breaking_degree %f\n",_czas,_ob->vPos.x,_ob->vPos.y,_ob->vPos.z,_ob->money,_ob->amount_of_fuel,_ob->F,_ob->wheel_turn_angle,_ob->breaking_degree);
	}
	//fclose(pl);
}

// losowanie liczby z rozkladu normalnego o zadanej sredniej i wariancji
float Randn(float srednia, float wariancja, long liczba_iter)
{
	//long liczba_iter = 10;  // im wiecej iteracji tym rozklad lepiej przyblizony
	float suma = 0;
	for (long i = 0; i < liczba_iter; i++)
		suma += (float)rand() / RAND_MAX;
	return (suma - (float)liczba_iter / 2) * sqrt(12 * wariancja / liczba_iter) + srednia;
}

void AutoPilot::ParametersSimAnnealing(long number_of_epochs, float krok_czasowy, float czas_proby)
{
	float T = 0.02,//100,
		wT = 0.99,
		c = 100000.0;

	float pz = 0.1;   // prawdopodobie�stwo zmiany parametru (�eby nie wszystkie si� zmienia�y ka�dorazowo) 

	//for (long p=0;p<number_of_params;p++) par[p] = 0.9;
	long gotowka_pop = 0;

	float delta_par[100];
	FILE* f = fopen("wyzarz_log.txt", "w");

	fprintf(f, "Start optymalizacji %d parametrow z wykorzystaniem symulowanego wyzarzania\n", number_of_params);
	for (long ep = 0; ep < number_of_epochs; ep++)
	{
		// losuje poprawki dla cz�ci parametr�w:
		for (long p = 0; p < number_of_params; p++)
			if ((float)rand() / RAND_MAX < pz)
				delta_par[p] = Randn(0, T, 10);
			else
				delta_par[p] = 0;

		if (ep > 0)
			for (long p = 0; p < number_of_params; p++)
				par[p] += delta_par[p];
		for (long i = 0; i < number_of_params; i++)
			fprintf(f, "par[%d] = %3.10f;\n", i, par[i]);
		Terrain t2;
		MovableObject* Obiekt = new MovableObject(&t2);
		Obiekt->planting_skills = 1.0;
		Obiekt->money_collection_skills = 1.0;
		Obiekt->fuel_collection_skills = 1.0;
		long gotowka_pocz = Obiekt->state.money;

		ControlTest(Obiekt, krok_czasowy, czas_proby);

		long gotowka = Obiekt->state.money - gotowka_pocz;

		float dE = gotowka - gotowka_pop;
		float p_akc = 1.0 / (1 + exp(-dE / (c * T)));
		fprintf(f, "epoka %d: T = %f, gotowka = %d, dE = %f, p_akc = %f\n", ep, T, gotowka, dE, p_akc);
		//if (gotowka > 15000) break;
		// akceptujemy lub odrzucamy
		if (((float)rand() / RAND_MAX < p_akc) || (ep == 0))
		{
			gotowka_pop = gotowka;

			fprintf(f, "sym.wyz-akceptacja, %d epoka: T=%f, gotowka = %d\n", ep, T, gotowka);
			char lanc[256];
			sprintf(lanc, "sym.wyz-akceptacja, %d epoka: T=%f, gotowka = %d", ep, T, gotowka);
			//SetWindowText(main_window, lanc);

		}
		else
		{
			for (long p = 0; p < number_of_params; p++) par[p] -= delta_par[p];
		}
		delete Obiekt;
		fclose(f);
		f = fopen("wyzarz_log.txt", "a");

		T *= wT;

	} // po epokach

	fprintf(f, "Koniec wyzarzania, koncowy wynik to %d gotowki\nOto koncowe wartosci:\n", gotowka_pop);
	for (long i = 0; i < number_of_params; i++)
		fprintf(f, "par[%d] = %3.10f;\n", i, par[i]);
	fclose(f);

}