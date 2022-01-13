# Collaborative-Computing-in-Distributed-Systems

<h5> Calcan Elena-Claudia <br/>
331CA</h5><br/>


S-a implementat un program distribuit in MPI in care procesele sunt grupate intr-o
topologie formata din 3 clustere, fiecare avand cate un coordonator si un numar
de procese worker ce realizeaza task-uri computationale.  
  
 <br/> 


### Stabilirea Topologiei
-------------------------------------------------------------------------
   
   • in implementarea programului, topologia este privita ca fiind alcatuita 
    din 3 grafuri neorientate, acestea reprezentand clusterele
    
   • nodurile grafurilor reprezinta procesele, iar o muchie intre doua noduri
    reprezinta canalul de comunicatie
    
   • comunicarea dintre procesele worker si coordonatorul sau se realizeaza daca
    daca exista o muchie (canal de comunicatie) intre cele 2 noduri din graf  
    
   • procesle coordonator comunica direct intre ele fara a verifica adiacenta lor  
   
   • coordonatorii iau din fisierul corespunzator lor procesele worker ce fac parte
    din cluster-ul lor si le adauga in lista de adiacenta  
    
   • pentru ca workerii sa afle care proces este coordonatorul lor, procesele coordonator
    fac send catre workeri cu id-ul lor  

   • procesele worker primesc id-ul coordonatorului si il adauga in lista de adiacenta, 
    facandu-se astfel un canal de comunicatie intre cele doua procese  
    
   • dupa ce fiecare worker isi cunoaste procesul coordonator, coordonatorii isi trimit
    lista lor cu procesele worker  
    
   • cand coordonatorii au aflat informatiile despre celelalte clustere, acestia trimit
    mai departe informatiile catre workeri  
    
<br/>

### Realizarea calculelor
--------------------------------------------------------------------------

   • coordonatorul 0 afla dimensiunea vectorului si il genereaza  
   
   • coordonatorul 0 imparte vectorul in functie de numarul de procese worker din fiecare
    cluster  
    
   • se afla numarul de itaratii pe care trebuie sa le faca obligatoriu

        requierd_iterations = vector_size / total_workers;

   • se calculeaza numarul de iteratii pentru fiecare cluster   

        iteration_per_culuster =  requierd_iterations * cluster_total_workers

   • in functie de numarul de elemente din vector care nu sunt asignate, se adauga
    in numarul de iteratii pe care il face un cluster  
    
   • coordonatorul 0 trimite vectorul impartit celorlalte clustere  
   
   • coordonatorii primesc bucata din vector pe care cluster-ul sau il prelucreaza  
   
   • coordonatorul imparte bucata primita in alte bucatele pe care le trimite workerilor  
   
   • impartirea task-urilor proceselor worker de catre coordonator se realizeaza prin
    calcularea unor indici de start si de end dupa urmatoarele formule

            start = worker_id * ((double)size / num_workers);
            end = MIN(size, (worker_id + 1) * ((double)size / num_workers));, unde

            worker_id = indicele procesului worker din lista de adiacenta
            size = numarul de element din vector pe care o itereaza cluster-ul respectiv
            num_workers = numarul total de workers din cluster 

   • workerii primesc partea lor din vector si isi realizeaza task-ul, rezultatele fiind
    trimise coordonatorului cand calulele sunt finalizate  
    
   • cand toti workerii dintr-un cluster isi termina task-ul, procesul coordonator trimite catre
    coordonatorul 0 rezultatele obtinute  
    
   • coordonatorul 0 primeste rezultatele clusterelor si le asambleaza  
   
   
