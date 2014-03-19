-module(fs).
-import(dispatcher, [inicio/1]).
-import(broadcaster, [crearBroad/0, suscribirse/0, msjSD/1, enviarMsj/1]).
-compile(export_all).

main() ->
    crearBroad(),
    P1 =spawn(?MODULE, crearfs, []),
    P2 =spawn(?MODULE, crearfs, []),
    P3 =spawn(?MODULE, crearfs, []),
    P4 =spawn(?MODULE, crearfs, []),
    P5 =spawn(?MODULE, crearfs, []),
    register(worker1, P1),
    register(worker2, P2),
    register(worker3, P3),
    register(worker4, P4),  
    register(worker5, P5),
    io:format("Servidor aceptando clientes\n"),
    inicio(8000).


crearfs() ->
    suscribirse(),
    fs([]).

fs(Buff) ->
    receive
        {cre,N,Pid}->
            X = enviarMsj({msj, cre, N}),
            Y = lists:filter(fun(A) -> A /= false end, X),
            Z = busca(N,Buff),
            if
                (length(Y) > 0) or Z ->
                    Pid ! {cre, error}, fs(Buff);
                true ->
                    Pid ! {cre, ok}, fs([{{cerrado, false},{nombre, N},{data, [],[],0}}|Buff])
            end;
        {msj,cre,N} -> 
            case busca(N,Buff) of
                false ->
                    msjSD(false);
                _ ->
                    msjSD(self())
            end,
            fs(Buff);
        {lsd, Pid} ->
            X = enviarMsj({msj, lsd}),
            Y = lists:concat([X,lists:map(fun({_,{_,A},_}) -> [$ |A] -- "\r\n" end, Buff),"\n"]),
            Pid ! {lsd, Y}, fs(Buff);
        {msj, lsd} ->
            X = lists:map(fun({_,{_,A},_}) -> [$ |A] -- "\r\n" end, Buff),
            Y = lists:append(X),
            msjSD(Y), fs(Buff);
        {del,N,Pid} ->
            case bb(N,Buff) of
                {_,error1} ->
                    X = enviarMsj({msj, del, N}),
                    Y = lists:filter(fun(A) ->A /= error1 end, X),
                    case Y of
                        [] ->
                            Pid ! {del, error1}, fs(Buff);
                        [Z] ->
                            Pid ! {del, Z}, fs(Buff)
                    end;
                {B,E} ->
                    Pid ! {del, E}, fs(B)
            end;
        {msj,del,N} ->
            case bb(N,Buff) of
                {X,ok} ->
                    msjSD(ok), fs(X);
                {_,Y} ->
                    msjSD(Y), fs(Buff)
            end;
        {opn,N,M,Pid} ->
            case abrir(N,M,Pid,Buff) of
                {_, error1}->      
                    X = enviarMsj({msj,opn,N,M,Pid}),
                    Y = lists:filter(fun(A) -> A /= error1 end, X),
                    case Y of
                        [] ->
                            Pid ! {opn, error1}, fs(Buff);
                        [P] ->
                            Pid ! {opn, P}, fs(Buff)
                    end;
                {B,E} ->
                    Pid ! {opn, E}, fs(B)
            end;
        {msj,opn,N,M,Pid} ->
            case abrir(N,M,Pid,Buff) of
                {B, E} -> msjSD(E), fs(B)
            end;
        {wrt,N,Size,S,P,Pid} when P == self()->
            X = lists:map(fun({A,{B,C},{D,E,F,G}}) -> if C == N -> {A,{B,C},{D,E++S,F,G+Size}}; true -> {A,{B,C},{D,E,F,G}} end end, Buff),
            Pid ! {wrt, ok},
            fs(X);
        {wrt,N,Size,S,P,Pid} ->
            P ! {msj,wrt,N,Size,S,self()},
            receive {msj, wrt, ok} -> Pid ! {wrt, ok} end, fs(Buff);
        {msj,wrt,N,Size,S,Pid} ->
            X = lists:map(fun({A,{B,C},{D,E,F,G}}) -> if C == N -> {A,{B,C},{D,E++S,F,G+Size}}; true -> {A,{B,C},{D,E,F,G}} end end, Buff),
            Pid ! {msj, wrt, ok},
            fs(X);
        {rea,N,P,Size,Pid} when P == self()->
            {B,R,S} = br(N,Size,Pid,Buff),
            Pid ! {rea, R, S},
            fs(B);
        {rea,N,P,Size,Pid} ->
            P ! {msj,rea,Pid,N,Size,self()},
            receive {msj,rea,R,S} -> Pid ! {rea,R,S} end,
            fs(Buff);
        {msj,rea,P,N,Size,Pid} ->
            {B,R,S} = br(N,Size,P,Buff),
            Pid ! {msj,rea,R,S},
            fs(B);
        {clo,N,M,P,Pid} when P == self()->
            X = cob(N,M,Pid,Buff),
            Pid ! {clo,ok},
            fs(X);
        {clo,N,M,P,Pid} ->
            P ! {msj, clo, N, M, Pid, self()},
            receive {msj,clo,ok} -> Pid ! {clo,ok} end,
            fs(Buff);
        {msj,clo,N,M,P,Pid} ->
            X = cob(N,M,P,Buff),
            Pid ! {msj,clo,ok},
            fs(X);
        {rm,N,Pid} ->
            case brm(N,Buff) of
                {_,error1} ->
                    X = enviarMsj({msj, rm, N}),
                    Y = lists:filter(fun(A) -> A /= error1 end, X),
                    case Y of
                        [] ->
                            Pid ! {rm, error1}, fs(Buff);
                        [Z] ->
                            Pid ! {rm, Z}, fs(Buff)
                    end;
                {B,E} ->
                    Pid ! {rm, E}, fs(B)

            end;
        {msj,rm,N} ->
            case brm(N,Buff) of
                {B,ok} ->
                    msjSD(ok), fs(B);
                {_,Y} ->
                    msjSD(Y), fs(Buff)
            end;
        _  -> 
            fs(Buff)
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
