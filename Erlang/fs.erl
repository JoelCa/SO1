-module(fs).
-import(dispatcher, [inicio/2]).
-compile(export_all).

main() ->
    iniciarSist(8000).

main(Puerto) ->
    P = list_to_integer(atom_to_list(lists:nth(1, Puerto))),
    iniciarSist(P).

iniciarSist(P) ->
    List = lanzarWorker(5,[]),
    distribuirPids(List),
    inicio(P,List).


lanzarWorker(0, List) ->
    P = spawn(?MODULE, crearfs, []),
    [P|List];
lanzarWorker(N, List) ->
    P = spawn(?MODULE, crearfs, []),
    lanzarWorker(N-1,[P|List]).

crearfs() ->
    receive
        {listwork, L} ->
            fs([],L,length(L),[])
    end.

%"Procesando" es una lista de la forma {ID, Pedido, Lista de resultados, La longitud de la esa lista}

fs(Buff,LW,LWLength,Processing) ->
    receive
                                                %CRE
        {cre,N,ID,Pid}->
            enviarMsj({msj, cre, N, ID, self()},LW),
            fs(Buff,LW,LWLength,[{ID, {cre, N, Pid}, [], 0}|Processing]);

        {msj,cre,N,ID,PW} -> 
            case busca(N,Buff) of
                false ->
                    PW ! {rmsj, ID, false};
                _ ->
                    PW ! {rmsj, ID, self()}
            end,
            fs(Buff,LW,LWLength,Processing);

                                                %LSD
        {lsd,ID,Pid} ->
            enviarMsj({msj, lsd, ID, self()},LW),
            fs(Buff,LW,LWLength,[{ID, {lsd, Pid}, [], 0}|Processing]);

        {msj,lsd,ID,PW} ->
            X = lists:map(fun({_,{_,A},_}) -> [$ |A] -- "\r\n" end, Buff),
            Y = lists:append(X),
            PW ! {rmsj, ID, Y}, fs(Buff,LW,LWLength,Processing);

                                                %DEL
        {del,N,ID,Pid} ->
            case bb(N,Buff) of
                {_,error1} ->
                    enviarMsj({msj, del, N, ID, self()},LW),
                    fs(Buff,LW,LWLength,[{ID, {del,N,Pid}, [], 0}|Processing]);
                {B,E} ->
                    Pid ! {del, E}, fs(B,LW,LWLength,Processing)
            end;

        {msj,del,N,ID,PW} ->
            case bb(N,Buff) of
                {X,ok} ->
                    PW ! {rmsj, ID, ok}, fs(X,LW,LWLength,Processing);
                {_,Y} ->
                    PW ! {rmsj, ID, Y}, fs(Buff,LW,LWLength,Processing)
            end;

                                                %OPN
        {opn,N,M,ID,Pid} ->
            case abrir(N,M,Pid,Buff) of
                {_, error1}->      
                    enviarMsj({msj,opn,N,M,Pid,ID,self()},LW),
                    fs(Buff,LW,LWLength,[{ID, {opn,N,M,Pid}, [], 0}|Processing]);
                {B,E} ->
                    Pid ! {opn, E}, fs(B,LW,LWLength,Processing)
            end;

        {msj,opn,N,M,Pid,ID,PW} ->
            case abrir(N,M,Pid,Buff) of
                {B, E} -> PW ! {rmsj, ID, E}, fs(B,LW,LWLength,Processing)
            end;


                                                %WRT
        {wrt,N,Size,S,P,Pid} when P == self()->
            X = lists:map(fun({A,{B,C},{D,E,F,G}}) -> if C == N -> {A,{B,C},{D,E++S,F,G+Size}}; true -> {A,{B,C},{D,E,F,G}} end end, Buff),
            Pid ! {wrt, ok},
            fs(X,LW,LWLength,Processing);

        {wrt,N,Size,S,P,Pid} ->
            P ! {msj,wrt,N,Size,S,self()},
            receive {msj, wrt, ok} -> Pid ! {wrt, ok} end, fs(Buff,LW,LWLength,Processing);

        {msj,wrt,N,Size,S,Pid} ->
            X = lists:map(fun({A,{B,C},{D,E,F,G}}) -> if C == N -> {A,{B,C},{D,E++S,F,G+Size}}; true -> {A,{B,C},{D,E,F,G}} end end, Buff),
            Pid ! {msj, wrt, ok},
            fs(X,LW,LWLength,Processing);

                                                %REA
        {rea,N,P,Size,Pid} when P == self()->
            {B,R,S} = br(N,Size,Pid,Buff),
            Pid ! {rea, R, S},
            fs(B,LW,LWLength,Processing);

        {rea,N,P,Size,Pid} ->
            P ! {msj,rea,Pid,N,Size,self()},
            receive {msj,rea,R,S} -> Pid ! {rea,R,S} end,
            fs(Buff,LW,LWLength,Processing);

        {msj,rea,P,N,Size,Pid} ->
            {B,R,S} = br(N,Size,P,Buff),
            Pid ! {msj,rea,R,S},
            fs(B,LW,LWLength,Processing);

                                                %CLO
        {clo,N,M,P,Pid} when P == self()->
            X = cob(N,M,Pid,Buff),
            Pid ! {clo,ok},
            fs(X,LW,LWLength,Processing);

        {clo,N,M,P,Pid} ->
            P ! {msj, clo, N, M, Pid, self()},
            receive {msj,clo,ok} -> Pid ! {clo,ok} end,
            fs(Buff,LW,LWLength,Processing);

        {msj,clo,N,M,P,Pid} ->
            X = cob(N,M,P,Buff),
            Pid ! {msj,clo,ok},
            fs(X,LW,LWLength,Processing);

                                                %RM
        {rm,N,ID,Pid} ->
            case brm(N,Buff) of
                {_,error1} ->
                    enviarMsj({msj, rm, N, ID, self()},LW),
                    fs(Buff,LW,LWLength,[{ID, {rm,N,Pid}, [], 0}|Processing]);
                {B,E} ->
                    Pid ! {rm, E}, fs(B,LW,LWLength,Processing)

            end;

        {msj,rm,N,ID,PW} ->
            case brm(N,Buff) of
                {B,ok} ->
                    PW ! {rmsj, ID, ok}, fs(B,LW,LWLength,Processing);
                {_,Y} ->
                    PW ! {rmsj, ID, Y}, fs(Buff,LW,LWLength,Processing)
            end;

        {rmsj,ID, X} ->
            case addReply({ID, X},Processing,LWLength) of
                {ok,Proce,Resp,P} ->
                    io:format("Tiene: ~p ~p\n",[P,Proce]),
                    reply(P,Resp,Buff,LW,LWLength,Proce);
                {foul, Proce} ->
                    fs(Buff,LW,LWLength,Proce)
            end;
        _  -> 
            fs(Buff,LW,LWLength,Processing)
    end.


                                                %Cierra o borra un archivo.
                                                %Definida para CLO.
cob(X,M,Pid,[{A,{B,X},C}|L]) ->
    case cambiarEstado(M,Pid,{A,{B,X},C}) of
        {{cerrado,true},_,_} ->
            L;
        Y ->
            [Y|L]
    end;
cob(X,M,Pid,[Y|L]) ->
    [Y|cob(X,M,Pid,L)].

cambiarEstado(l,Pid,{{lye,B},C,{D,E,F,G}}) ->
    {{lye,B},C,{D,E,borrarPid(Pid,F),G}};
cambiarEstado(l,Pid,{{l,B},C,{D,E,F,G}}) when length(F) == 1 ->
    {{cerrado,B},C,{D,E,borrarPid(Pid,F),G}};
cambiarEstado(l,Pid,{{l,B},C,{D,E,F,G}}) ->
    {{l,B},C,{D,E,borrarPid(Pid,F),G}};
cambiarEstado(lye,Pid,{{lye,B},C,{D,E,F,G}}) when length(F) == 1->
    {{cerrado,B},C,{D,E,borrarPid(Pid,F),G}};
cambiarEstado(lye,Pid,{{lye,B},C,{D,E,F,G}}) ->
    {{l,B},C,{D,E,borrarPid(Pid,F),G}}.

                                                %Setea la bandera para borrar un archivo cuando se cierre,
                                                %o puede borrarlo si ya está cerrado.
                                                %Definida para RM.
brm(_,[]) -> 
    {[],error1};
brm(X,[{{cerrado,_},{_,X},_}|L]) ->
    {L,ok};
brm(X,[{{A,false},{B,X},{C,D,E,F}}|L]) ->
    {[{{A,true},{B,X},{C,D,E,F}}|L], ok};
brm(X,[{{A,B},{C,X},D}|L]) ->
    {[{{A,B},{C,X},D}|L], error2};
brm(X,[Y|L]) ->
    case brm(X,L) of
        {A,B} -> 
            {[Y|A],B}
    end.


borrarPid(_,[]) ->
    [];
borrarPid(P, [{P,_}|T]) ->
    T;
borrarPid(P,[X|T]) ->
    [X|borrarPid(P,T)].


                                                %Determina si un archivo está en la lista de archivos.
                                                %Definida para CRE.
busca(_,[]) ->
    false;
busca(X,[{_,{_,X},_}|_]) ->
    true;
busca(X,[_|T]) ->
    busca(X,T).


                                                %Borra un archivo.
                                                %Definida para DEL.
bb(_,[]) -> 
    {[],error1};
bb(X,[{{cerrado,_},{_,X},_}|L]) ->
    {L,ok};
bb(X,[{A,{B,X},C}|L]) ->
    {[{A,{B,X},C}|L], error2};
bb(X,[Y|L]) ->
    case bb(X,L) of
        {A,B} -> 
            {[Y|A],B}
    end.

                                                %Retorna una tripleta. En su 2º componente, se encuentra
                                                %la porción de texto que se quiere leer de un archivo.
                                                %Definida para REA.
br(X,S,Pid,[{A,{_,X},{_,B,C,D}}|L]) ->
    List = lists:filter(fun({P,_}) -> P == Pid end, C),
    case List of
        [{_,Y}] ->
            if
                Y > D ->
                    {[{A,{nombre,X},{data,B,C,D}}|L], [],0};
                true ->
                    Z = string:substr(B,Y,S),
                    T = erlang:min(S,D-Y+1),
                    {[{A,{nombre,X},{data,B,lists:map(fun({P,I})-> if P == Pid -> {P, I+T}; true -> {P,I} end end, C),D}}|L],Z,T}
            end
    end;
br(X,S,Pid,[Y|L]) ->
    case br(X,S,Pid,L) of
        {A,B,C} ->
            {[Y|A],B,C}
    end.

                                                %Abre un archivo, dado por su nombre, en el 1º arg.
                                                %Definida para OPN.
abrir(_,_,_,[]) ->
    {[],error1};
abrir(X,lye,_,[{{lye,A},{B,X},C}|L]) ->
    {[{{lye,A},{B,X},C}|L], error2};
abrir(X,l,P,[{{lye,A},{B,X},{C,D,E,F}}|L]) ->
    {[{{lye,A},{B,X},{C,D,[{P,1}|E],F}}|L], self()};
abrir(X,M,P,[{{_,A},{B,X},{C,D,E,F}}|L]) ->
    {[{{M,A},{B,X},{C,D,[{P,1}|E],F}}|L], self()};
abrir(X,M,P,[Y|L]) ->
    case abrir(X,M,P,L) of
        {A,B} -> {[Y|A],B}
    end.



%%%%%
enviarMsj2(M,L) ->
    enviarMsj(M,L),
    recibirMsj(length(L)).

                                                %enviarMsj envía una consulta M a los demás workers
enviarMsj(M,[P|L]) ->
    P ! M,
    enviarMsj(M,L);
enviarMsj(_,[]) ->
    ok.

recibirMsj(0) -> [];
recibirMsj(N) -> receive {rmsj, X} -> [X|recibirMsj(N-1)] end.

                                                %Envía a cada worker, los pids de los demás
distribuirPids(L) -> distribuirPids(L,length(L)).

distribuirPids(_,0) ->
    ok;
distribuirPids(L,N) ->
    P = lists:nth(N,L),
    P ! {listwork, lists:filter(fun(A) -> A /= P end, L)},
    distribuirPids(L,N-1).


%{ID, Pedido, Lista de resultados, La longitud de la esa lista}
                                                %addReply, añade la respuesta a su lista de pedidos,
                                                %si corresponde a algún pedido pendiente.
                                                %En el caso que la respuesta sea la última, elimina el pedido de su lista.
addReply(_,[],_) ->
    {foul,[]};
addReply({ID, X},[{ID,Pedido,LP,M}|Processing],N) ->
    if
        M+1 < N ->
            {foul,[{ID,Pedido,[X|LP],M+1}|Processing]};
        true ->
            {ok,Processing,[X|LP],Pedido}
    end;
addReply(Tuple,[X|Processing],N) ->
    case addReply(Tuple,Processing,N) of
        {foul, Pro} ->
            {foul, [X|Pro]};
        {ok,Proce,LP,P} -> 
            {ok, [X|Proce], LP, P}
    end.

                                                %Aplica el operador que corresponde, luego obtener
                                                %las respuestas de los otros workers

reply({cre,N,Pid},Resp,Buff,LW,LWLength,Processing) ->
    Y = lists:filter(fun(A) -> A /= false end, Resp),
    Z = busca(N,Buff),
    if
        (length(Y) > 0) or Z ->
            Pid ! {cre, error}, fs(Buff,LW,LWLength,Processing);
        true ->
            Pid ! {cre, ok}, fs([{{cerrado, false},{nombre, N},{data, [],[],0}}|Buff],LW,LWLength,Processing)
    end;

reply({lsd,Pid},Resp,Buff,LW,LWLength,Processing) ->
    Y = lists:concat([Resp,lists:map(fun({_,{_,A},_}) -> [$ |A] -- "\r\n" end, Buff),"\n"]),
    Pid ! {lsd, Y},
    fs(Buff,LW,LWLength,Processing);

reply({del,_,Pid},Resp,Buff,LW,LWLength,Processing) ->
    Y = lists:filter(fun(A) ->A /= error1 end, Resp),
    case Y of
        [] ->
            Pid ! {del, error1}, fs(Buff,LW,LWLength,Processing);
        [Z] ->
            Pid ! {del, Z}, fs(Buff,LW,LWLength,Processing)
    end;

reply({opn,_,_,Pid},Resp,Buff,LW,LWLength,Processing) ->
    Y = lists:filter(fun(A) -> A /= error1 end, Resp),
    case Y of
        [] ->
            Pid ! {opn, error1}, fs(Buff,LW,LWLength,Processing);
        [P] ->
            Pid ! {opn, P}, fs(Buff,LW,LWLength,Processing)
    end;

reply({rm,_,Pid},Resp,Buff,LW,LWLength,Processing) ->
    Y = lists:filter(fun(A) -> A /= error1 end, Resp),
    case Y of
        [] ->
            Pid ! {rm, error1}, fs(Buff,LW,LWLength,Processing);
        [Z] ->
            Pid ! {rm, Z}, fs(Buff,LW,LWLength,Processing)
    end.
            
