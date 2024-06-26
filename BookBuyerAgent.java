/*  Klasa agenta kupującego książki w imieniu właściciela
 *
 *  parametry linii uruchomienia:
 *  -agents seller1:BookSellerAgent();seller2:BookSellerAgent();buyer1:BookBuyerAgent(Zamek) -gui
 */

import jade.core.Agent;
import jade.core.AID;
import jade.core.behaviours.*;
import jade.lang.acl.*;

// Przykładowa klasa zachowania:
class MyOwnBehaviour extends Behaviour
{
  protected MyOwnBehaviour()
  {
  }

  public void action()
  {
  }
  public boolean done() {
    return false;
  }
}

public class BookBuyerAgent extends Agent {

    private String targetBookTitle;    // tytuł kupowanej książki przekazywany poprzez argument wejściowy
    // lista znanych agentów sprzedających książki (w przypadku użycia żółtej księgi - usługi katalogowej, sprzedawcy
    // mogą być dołączani do listy dynamicznie!
    private AID[] sellerAgents = {
      new AID("seller1", AID.ISLOCALNAME),
      new AID("seller2", AID.ISLOCALNAME)};

    // Inicjalizacja klasy agenta:
    protected void setup()
    {

      //doWait(6000);   // Oczekiwanie na uruchomienie agentów sprzedających

      System.out.println("Witam! Agent-kupiec "+getAID().getName()+" (wersja c <2023/24>) jest gotów!");

      Object[] args = getArguments();  // lista argumentów wejściowych (tytuł książki)

      if (args != null && args.length > 0)   // jeśli podano tytuł książki
      {
        targetBookTitle = (String) args[0];
        System.out.println("Zamierzam kupić książkę zatytułowaną "+targetBookTitle);

        addBehaviour(new RequestPerformer());  // dodanie głównej klasy zachowań - kod znajduje się poniżej

      }
      else
      {
        // Jeśli nie przekazano poprzez argument tytułu książki, agent kończy działanie:
        System.out.println("Proszę podać tytuł lektury w argumentach wejściowych agenta kupującego!");
        doDelete();
      }
    }
    // Metoda realizująca zakończenie pracy agenta:
    protected void takeDown()
    {
      System.out.println("Agent-kupiec "+getAID().getName()+" kończy.");
    }

    /**
    Inner class RequestPerformer.
    This is the behaviour used by Book-buyer agents to request seller
    agents the target book.
    */
    private class RequestPerformer extends Behaviour
    {

      private AID bestSeller;     // agent sprzedający z najkorzystniejszą ofertą
      private double bestPrice;      // najlepsza cena
      private int repliesCnt = 0; // liczba odpowiedzi od agentów
      private int targowanieCnt = 0; // liczba ile razy targował się kupujący
      private int targowaneieMax = 6; // liczba ile razy maksymalnie kupiec będzie się targował
      private MessageTemplate mt; // szablon odpowiedzi
      private int step = 0;       // krok

      public void action()
      {
        switch (step) {
        case 0:      // wysłanie oferty kupna
          System.out.print(" Oferta kupna (CFP) jest wysyłana do: ");
          for (int i = 0; i < sellerAgents.length; ++i)
          {
            System.out.print(sellerAgents[i]+ " ");
          }
          System.out.println();

          // Tworzenie wiadomości CFP do wszystkich sprzedawców:
          ACLMessage cfp = new ACLMessage(ACLMessage.CFP);
          for (int i = 0; i < sellerAgents.length; ++i)
          {
            cfp.addReceiver(sellerAgents[i]);                // dodanie adresata
          }
          cfp.setContent(targetBookTitle);                   // wpisanie zawartości - tytułu książki
          cfp.setConversationId("handel_ksiazkami");         // wpisanie specjalnego identyfikatora korespondencji
          cfp.setReplyWith("cfp"+System.currentTimeMillis()); // dodatkowa unikatowa wartość, żeby w razie odpowiedzi zidentyfikować adresatów
          myAgent.send(cfp);                           // wysłanie wiadomości

          // Utworzenie szablonu do odbioru ofert sprzedaży tylko od wskazanych sprzedawców:
          mt = MessageTemplate.and(MessageTemplate.MatchConversationId("handel_ksiazkami"),
                                   MessageTemplate.MatchInReplyTo(cfp.getReplyWith()));
          step = 1;     // przejście do kolejnego kroku
          break;
        case 1:      // odbiór ofert sprzedaży/odmowy od agentów-sprzedawców
          ACLMessage reply = myAgent.receive(mt);      // odbiór odpowiedzi
          if (reply != null)
          {
            if (reply.getPerformative() == ACLMessage.PROPOSE)   // jeśli wiadomość jest typu PROPOSE
            {
              double price = Double.parseDouble(reply.getContent());  // cena książki
              if (bestSeller == null || price < bestPrice)       // jeśli jest to najlepsza oferta
              {
                bestPrice = price;
                bestSeller = reply.getSender();
              }
            }
            repliesCnt++;                                        // liczba ofert
            if (repliesCnt >= sellerAgents.length)               // jeśli liczba ofert co najmniej liczbie sprzedawców
            {
              bestPrice = bestPrice * 0.5;
              step = 2;
            }
          }
          else
          {
            block();
          }
          break;
        case 2:
          ACLMessage targowanie = new ACLMessage(ACLMessage.REQUEST);
          if (targowanieCnt < targowaneieMax) {
            if (targowanieCnt != 0) {
              bestPrice = bestPrice + 5;
            }
            targowanie.setContent(String.valueOf(bestPrice));
            targowanie.addReceiver(bestSeller);
            targowanie.setConversationId("targowanie");
            targowanie.setReplyWith("kupiec_targowanie");
            myAgent.send(targowanie);

            System.out.println("Agent-klient targuje się " + targowanieCnt + " razy z ceną " + bestPrice);

            mt = MessageTemplate.and(MessageTemplate.MatchConversationId("targowanie"),
                    MessageTemplate.MatchInReplyTo(targowanie.getReplyWith()));
            targowanieCnt++;
            step = 3;
          } else {
            step = 6;
          }
          break;
        case 3:
          reply = myAgent.receive(mt);
          if (reply != null) {
            if (reply.getPerformative() == ACLMessage.REQUEST_WHEN) {
              double price = Double.parseDouble(reply.getContent());
              if (price - bestPrice < 2) {
                bestPrice = price;
                step = 4;
              }
              else {
                step = 2;
              }
            } else {
              block();
            }
          }

          break;

        case 4:      // wysłanie zamówienia do sprzedawcy, który złożył najlepszą ofertę
          ACLMessage order = new ACLMessage(ACLMessage.ACCEPT_PROPOSAL);
          order.addReceiver(bestSeller);
          order.setContent(targetBookTitle);
          order.setConversationId("koniec_handlu");
          order.setReplyWith("order"+System.currentTimeMillis());
          myAgent.send(order);
          mt = MessageTemplate.and(MessageTemplate.MatchConversationId("koniec_handlu"),
                                   MessageTemplate.MatchInReplyTo(order.getReplyWith()));
          step = 5;
          break;
        case 5:      // odbiór odpowiedzi na zamównienie
          reply = myAgent.receive(mt);
          if (reply != null)
          {
            if (reply.getPerformative() == ACLMessage.INFORM)
            {
              System.out.println("Tytuł "+targetBookTitle+" zamówiony!");
              System.out.println("Po cenie: "+bestPrice);
              myAgent.doDelete();
            }
            step = 8;
          }
          else
          {
            block();
          }
          break;
        case 6:
          ACLMessage refusal = new ACLMessage(ACLMessage.REJECT_PROPOSAL);
          refusal.addReceiver(bestSeller);
          refusal.setContent(targetBookTitle);
          refusal.setConversationId("koniec_handlu");
          refusal.setReplyWith("odmowa" + System.currentTimeMillis());
          myAgent.send(refusal);
          mt = MessageTemplate.and(MessageTemplate.MatchConversationId("odmowa"),
                  MessageTemplate.MatchInReplyTo(refusal.getReplyWith()));
          step = 7;
          break;
        case 7:
          reply = myAgent.receive(mt);
          if (reply != null)
          {
            if (reply.getPerformative() == ACLMessage.INFORM_REF)
            {
              System.out.println("Handel został zakończony niepowodzeniem");
              myAgent.doDelete();
            }
            step = 8;
          }
          else
          {
            block();
          }
          break;
        }  // switch
      } // action

      public boolean done() {
        return ((step == 2 && bestSeller == null) || step == 8);
      }
    } // Koniec wewnętrznej klasy RequestPerformer
}
