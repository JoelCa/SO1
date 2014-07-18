-module(fs).
-import(dispatcher, [inicio/2]).
-compile(export_all).

main() ->
    iniciarSist(8000).

main(Puerto) ->
    P = list_to_integer(atom_to_list(lists:nth(1, Puerto))),
    iniciarSist(P).

iniciarSist(P) ->
    P1 =spawn(?MODULE, crearfs, []),
    P2 =spawn(?MODULE, crearfs, []),
    P3 =spawn(?MODULE, crearfs, []),
    P4 =spawn(?MODULE, crearfs, []),
    P5 =spawn(?MODULE, crearfs, []),
    enviarPids([P1,P2,P3,P4,P5]),
    inicio(P,[{P1,0},{P2,0},{P3,0},{P4,0},{P5,0}]).

crearfs() ->
    receive
        {listwork, L} ->
            fs([],L)
    end.

fs(Buff,LW) ->
    receive
        {cre,N,Pid}->
            X = enviarMsj({msj, cre, N, self()},LW),
            Y = lists:filter(fun(A) -> A /= false end, X),
            Z = busca(N,Buff),
            if
                (length(Y) > 0) or Z ->
                    Pid ! {cre, error}, fs(Buff,LW);
                true ->
                    Pid ! {cre, ok}, fs([{{cerrado, false},{nombre, N},{data, [],[],0}}|Buff],LW)
            end;
        {msj,cre,N,PW} -> 
            case busca(N,Buff) of
                false ->
                    PW ! {rmsj, false};
                _ ->
                    PW ! {rmsj, self()}
            end,
            fs(Buff,LW);
        {lsd,Pid} ->
            X = enviarMsj({msj, lsd, self()},LW),
            Y = lists:concat([X,lists:map(fun({_,{_,A},_}) -> [$ |A] -- "\r\n" end, Buff),"\n"]),
            Pid ! {lsd, Y},
            fs(Buff,LW);
        {msj,lsd,PW} ->
            X = lists:map(fun({_,{_,A},_}) -> [$ |A] -- "\r\n" end, Buff),
            Y = lists:append(X),
            PW ! {rmsj, Y}, fs(Buff,LW);
        {del,N,Pid} ->
            case bb(N,Buff) of
                {_,error1} ->
                    X = enviarMsj({msj, del, N, self()},LW),
                    Y = lists:filter(fun(A) ->A /= error1 end, X),
                    case Y of
                        [] ->
                            Pid ! {del, error1}, fs(Buff,LW);
                        [Z] ->
                            Pid ! {del, Z}, fs(Buff,LW)
                    end;
                {B,E} ->
                    Pid ! {del, E}, fs(B,LW)
            end;
        {msj,del,N,PW} ->
            case bb(N,Buff) of
                {X,ok} ->
                    PW ! {rmsj, ok}, fs(X,LW);
                {_,Y} ->
                    PW ! {rmsj, Y}, fs(Buff,LW)
            end;
        {opn,N,M,Pid} ->
            case abrir(N,M,Pid,Buff) of
                {_, error1}->      
                    X = enviarMsj({msj,opn,N,M,Pid,self()},LW),
                    Y = lists:filter(fun(A) -> A /= error1 end, X),
                    case Y of
                        [] ->
                            Pid ! {opn, error1}, fs(Buff,LW);
                        [P] ->
                            Pid ! {opn, P}, fs(Buff,LW)
                    end;
                {B,E} ->
                    Pid ! {opn, E}, fs(B,LW)
            end;
        {msj,opn,N,M,Pid,PW} ->
            case abrir(N,M,Pid,Buff) of
                {B, E} -> PW ! {rmsj, E}, fs(B,LW)
            end;
        {wrt,N,Size,S,P,Pid} when P == self()->
            X = lists:map(fun({A,{B,C},{D,E,F,G}}) -> if C == N -> {A,{B,C},{D,E++S,F,G+Size}}; true -> {A,{B,C},{D,E,F,G}} end end, Buff),
            Pid ! {wrt, ok},
            fs(X,LW);
        {wrt,N,Size,S,P,Pid} ->
            P ! {msj,wrt,N,Size,S,self()},
            receive {msj, wrt, ok} -> Pid ! {wrt, ok} end, fs(Buff,LW);
        {msj,wrt,N,Size,S,Pid} ->
            X = lists:map(fun({A,{B,C},{D,E,F,G}}) -> if C == N -> {A,{B,C},{D,E++S,F,G+Size}}; true -> {A,{B,C},{D,E,F,G}} end end, Buff),
            Pid ! {msj, wrt, ok},
            fs(X,LW);
        {rea,N,P,Size,Pid} when P == self()->
            {B,R,S} = br(N,Size,Pid,Buff),
            Pid ! {rea, R, S},
            fs(B,LW);
        {rea,N,P,Size,Pid} ->
            P ! {msj,rea,Pid,N,Size,self()},
            receive {msj,rea,R,S} -> Pid ! {rea,R,S} end,
            fs(Buff,LW);
        {msj,rea,P,N,Size,Pid} ->
            {B,R,S} = br(N,Size,P,Buff),
            Pid ! {msj,rea,R,S},
            fs(B,LW);
        {clo,N,M,P,Pid} when P == self()->
            X = cob(N,M,Pid,Buff),
            Pid ! {clo,ok},
            fs(X,LW);
        {clo,N,M,P,Pid} ->
            P ! {msj, clo, N, M, Pid, self()},
            receive {msj,clo,ok} -> Pid ! {clo,ok} end,
            fs(Buff,LW);
        {msj,clo,N,M,P,Pid} ->
            X = cob(N,M,P,Buff),
            Pid ! {msj,clo,ok},
            fs(X,LW);
        {rm,N,Pid} ->
            case brm(N,Buff) of
                {_,error1} ->
                    X = enviarMsj({msj, rm, N, self()},LW),
                    Y = lists:filter(fun(A) -> A /= error1 end, X),
                    case Y of
                        [] ->
                            Pid ! {rm, error1}, fs(Buff,LW);
                        [Z] ->
                            Pid ! {rm, Z}, fs(Buff,LW)
                    end;
                {B,E} ->
                    Pid ! {rm, E}, fs(B,LW)

            end;
        {msj,rm,N,PW} ->
            case brm(N,Buff) of
                {B,ok} ->
                    PW ! {rmsj, ok}, fs(B,LW);
                {_,Y} ->
                    PW ! {rmsj, Y}, fs(Buff,LW)
            end;
        _  -> 
            fs(Buff,LW)
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

                                                %Busca y setea la bandera , o puede borrar.
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


                                                %Determina si el nombre del 1º arg. esta en la 2º arg.
                                                %Definida para CRE.
busca(_,[]) ->
    false;
busca(X,[{_,{_,X},_}|_]) ->
    true;
busca(X,[_|T]) ->
    busca(X,T).


                                                %Busca y borra.
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

                                                %Busca y reemplaza.
                                                %Definida para WRT
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

                                                %Abre el archivo dado en el 1º arg.
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

                                                %enviarMsj envia la consulta M a los demas worker, y
                                                %devuelve una lista con las respuestas
enviarMsj(M,L) ->
    enviarM(M,L),
    recibirMsj(length(L)).

enviarM(M,[P|L]) ->
    P ! M,
    enviarM(M,L);
enviarM(_,[]) ->
    ok.

recibirMsj(0) -> [];
recibirMsj(N) -> receive {rmsj, X} -> [X|recibirMsj(N-1)] end.

enviarPids(L) -> enviarPids(L,length(L)).

enviarPids(_,0) ->
    ok;
enviarPids(L,N) ->
    P = lists:nth(N,L),
    P ! {listwork, lists:filter(fun(A) -> A /= P end, L)},
    enviarPids(L,N-1).
