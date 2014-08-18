-module(dispatcher).
-compile(export_all).

inicio(Port,ListPids) ->
    {ok, ServerSocket} = gen_tcp:listen(Port,[{active,false}]),
    P1 = spawn (?MODULE, round, [ListPids, length(ListPids), 1, 0]),
    register(workerselector, P1),
    P2 = spawn(?MODULE, descriptor,[[],1]),
    register(desc, P2),
    io:format("Servidor aceptando clientes\n"),
    serv(ServerSocket).

serv(ServerSocket) ->
    {ok, ClientSocket} = gen_tcp:accept(ServerSocket),
    P = spawn (?MODULE, proc_socket, [ClientSocket,{sinworker,-1},[]]),
    gen_tcp:controlling_process(ClientSocket, P),
    serv(ServerSocket).

proc_socket(ClientS,Tuple,List) ->
    case gen_tcp:recv(ClientS,0) of
        {ok, Bin} ->
            proc_socket(ClientS,Bin,Tuple,List);
        {error,closed} ->
            case Tuple of
                {sinworker,_} ->
                    ok;
                {_, ID} -> 
                    salir(List),
                    io:format("<- Cliente con ID ~p desconectado\n", [ID])
                    %% receive
                    %%     {numworker, N} ->
                    %%         io:format("Cliente desconectado: ID ~p\n", [N])
                    %% end
            end
    end.


proc_socket(ClientS,Bin,{sinworker,_},List) ->
    case string:sub_word(Bin,1) of
        "CON\r\n"->
            workerselector ! {getworker, self()},
            receive
                {worker, P, X} ->
                    gen_tcp:send(ClientS, lists:concat(["OK ID ",X, "\n"])),
                    io:format("-> Cliente con ID ~p conectado\n", [X]),
                    proc_socket(ClientS, {P,X}, List);
                {workererror} ->
                    gen_tcp:send(ClientS, "ERROR SERVIDOR SATURADO\n"), proc_socket(ClientS, sinworker, List)
            end;
        _ -> proc_socket(ClientS,{sinworker,-1},List)
    end;
proc_socket(ClientS,T,{W,ID},List) ->
    Tokens = string:tokens(T, " "),
    case lists:nth(1,Tokens) of
        "CON\r\n" ->
            gen_tcp:send(ClientS, "ERROR YA ESTA CONECTADO\n"), proc_socket(ClientS, {W,ID},List);
        "BYE\r\n" ->
            salir(List),
            gen_tcp:send(ClientS, "OK\n"),
            gen_tcp:close(ClientS),
            receive
                {numworker, N} ->
                    io:format("Cliente desconectado: worker nº ~p\n", [N])
            end;
        "CRE"  ->
             if
                length(Tokens) > 2  ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                true ->
                    X = lists:nth(2,Tokens),
                    if
                        X == "\r\n" ->
                            gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                        true ->
                            W ! {cre, X, ID, self()},
                            receive 
                                {cre, ok} ->
                                    gen_tcp:send(ClientS, "OK\n");
                                {cre, error} ->
                                    gen_tcp:send(ClientS, "ERROR EXISTE UN ARCHIVO CON ESE NOMBRE\n")
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end
                    end
            end,
            proc_socket(ClientS, {W,ID}, List);
        "LSD\r\n"->
            W ! {lsd, self()},
            receive
                {lsd, X} ->
                    gen_tcp:send(ClientS, lists:concat(["OK",X]))
            after 100 ->
                    io:format("ERROR WORKER NO RESPONDE\n")
            end,
            proc_socket(ClientS, {W,ID}, List);
        "DEL"  ->
            if
                length(Tokens) > 2 ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                true ->
                    X = lists:nth(2,Tokens),
                    if
                        X == "\r\n" ->
                            gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                        true ->
                            W ! {del, X, self()},
                            receive
                                {del,ok} ->
                                    gen_tcp:send(ClientS, "OK\n");
                                {del,error1} ->
                                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO NO EXISTE\n");
                                {del,error2} ->
                                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO ESTA ABIERTO\n")
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end
                    end
            end,
            proc_socket(ClientS, {W,ID}, List);
        "OPN"  ->
            case analizar_opn(Tokens) of
                error1 ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n"), proc_socket(ClientS, {W,ID}, List);
                {M,X} ->
                    Y = lists:filter(fun({A,_,_}) -> A == X end, List),
                    case Y of
                        [] ->
                            W ! {opn, X, M, self()},
                            receive
                                {opn, error1} ->
                                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO NO EXISTE\n"), proc_socket(ClientS, {W,ID}, List);
                                {opn, error2} ->
                                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO ESTA ABIERTO EN MODO RDWR POR OTRO USUARIO\n"), proc_socket(ClientS, {W,ID}, List);
                                {opn, P} ->
                                    desc ! {des, ins, X, P, self()},
                                    receive {des, I} -> gen_tcp:send(ClientS, lists:concat(["OK FD ",I,"\n"])) end,
                                    proc_socket(ClientS, {W,ID}, [{X,M,P}|List])
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end;
                        _ ->
                            gen_tcp:send(ClientS, "ERROR EL ARCHIVO ESTA ABIERTO\n"), proc_socket(ClientS, {W,ID}, List)
                    end
            end;
        "WRT"  ->
            case analizar(Tokens, wrt) of
                error1  ->
                    gen_tcp:send(ClientS, "ERROR TAMAÑO INCORRECTO\n");
                error2  ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                error3 ->
                    gen_tcp:send(ClientS, "ERROR FD INCORRECTO\n");
                error4 ->
                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO ESTA ABIERTO POR OTRO USUARIO\n");
                {N,Size,P,S} ->
                    {_,M,_} = lists:nth(1,lists:filter(fun({A,_,_}) -> A == N end, List)),
                    if
                        M == l ->
                            gen_tcp:send(ClientS, "ERROR ARCHIVO ABIERTO EN MODO RDONLY\n");
                        true ->
                            W ! {wrt,N,Size,S,P,self()},
                            receive
                                {wrt, ok} ->
                                    gen_tcp:send(ClientS,"OK\n")
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end
                    end
            end,
            proc_socket(ClientS, {W,ID}, List);
        "REA"  ->
            case analizar(Tokens, rea) of
                error2 ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                error3 ->
                    gen_tcp:send(ClientS, "ERROR FD INCORRECTO\n");
                error4 ->
                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO ESTA ABIERTO POR OTRO USUARIO\n");
                {N,P,Size} ->
                    W ! {rea,N,P,Size,self()},
                    receive
                        {rea,R,S} ->
                            gen_tcp:send(ClientS, lists:concat(["OK SIZE ",S," ",R,"\n"]))
                    after 100 ->
                            io:format("ERROR WORKER NO RESPONDE\n")
                    end
            end,
            proc_socket(ClientS, {W,ID}, List);
        "CLO" ->
            X = lists:nth(2,Tokens),
            if
                length(Tokens) /= 3  ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n"),
                    proc_socket(ClientS, {W,ID}, List);
                true ->
                    FD = fst(string:to_integer(lists:nth(3,Tokens))),
                    if
                        is_integer(FD) and (FD > 0) and (X == "FD") ->
                            case nd(FD,1) of
                                error3 ->
                                    gen_tcp:send(ClientS, "ERROR FD INCORRECTO\n"),
                                    proc_socket(ClientS, {W,ID}, List);
                                error4 ->
                                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO ESTA ABIERTO POR OTRO USUARIO\n"),
                                    proc_socket(ClientS, {W,ID}, List);
                                {N,P} ->
                                    [{A,M,B}] = lists:filter(fun({A,_,_}) -> A == N end, List),
                                    W ! {clo, N, M, P, self()},
                                    receive
                                        {clo, ok} -> gen_tcp:send(ClientS, "OK\n")
                                    after 100 ->
                                            io:format("ERROR WORKER NO RESPONDE\n")
                                    end,
                                    proc_socket(ClientS, {W,ID}, List--[{A,M,B}])
                            end;
                        true  ->
                            gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n"),
                            proc_socket(ClientS, {W,ID}, List)
                    end
            end;
        "RM" ->
            if
                length(Tokens) > 2 ->
                    gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                true ->
                    X = lists:nth(2,Tokens),
                    if
                        X == "\r\n" ->
                            gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n");
                        true ->
                            W ! {rm, X, self()},
                            receive
                                {rm,ok} ->
                                    gen_tcp:send(ClientS, "OK\n");
                                {rm,error1} ->
                                    gen_tcp:send(ClientS, "ERROR EL ARCHIVO NO EXISTE\n");
                                {rm,error2} ->
                                    gen_tcp:send(ClientS, "ERROR PEDIDO YA REALIZADO\n")
                            after 100 ->
                                    io:format("ERROR WORKER NO RESPONDE\n")
                            end
                    end
            end,
            proc_socket(ClientS, {W,ID}, List);
        _  -> 
            gen_tcp:send(ClientS, "ERROR DE SINTAXIS\n"), proc_socket(ClientS, {W,ID}, List)
    end.

                                                %round, mantiene una lista con los pids de los workers,
                                                %para asociarlos con los procesos socket. Además mantiene
                                                %un contador que será el ID del cliente.
                                                %Los workers se asignan en ronda.
round(L, N, I, ID) ->
    receive
        {getworker, Pid} ->
            Pid ! {worker, lists:nth(I, L), ID},
            round(L, N, (I rem N) + 1, ID +1)
    end.


borrarDes(N,P,[{N,[{P,_}],_}|L]) ->
    L;
borrarDes(N,P,[{N,A,B}|L]) ->
    [{N,lists:filter(fun({C,_}) -> P /= C end, A),B}|L];
borrarDes(N,P,[A|L]) ->
    [A|borrarDes(N,P,L)].

                                                %"Buff" es una lista de tuplas {N, I, P}:
                                                %-N corresponde al nombre del archivo.
                                                %-I es una lista de tuplas {A, B}.
                                                %   Donde A, es el pid del worker que tiene el archivo N abierto,
                                                %   y B es el FD asociado.
                                                %-P es el Pid del worker que tiene el archivo.
                                                %En el 3º caso del receive, "Flag" es una bandera
                                                %con el fin de identificar si es invocada por el operador CLO.
                                                %La utilizamos para obtener los descriptores de los archivos,
                                                %y el pid del worker que tiene un determinado archivo.
descriptor(Buff, I) ->
    receive 
        {des, ins, N, P, Pid} ->
            Pid ! {des, I},
            X  = lists:filter(fun({A,_,_}) -> A == N end, Buff),
            case X of
                [] ->
                    descriptor([{N,[{Pid,I}],P}|Buff],I+1);
                _ ->
                    descriptor(lists:map(fun({A,B,C}) -> if A == N -> {A,[{Pid,I}|B],C}; true -> {A,B,C} end end, Buff), I+1)
            end;
        {des, borrar, N, Pid} ->
            X = borrarDes(N,Pid,Buff),
            Pid ! {des,ok},
            descriptor(X,I);
        {des, Flag, FD, Pid} ->
            X = lists:filter(fun({_,A,_}) -> B = lists:filter(fun({_,D}) -> D == FD end, A), if length(B)>0 -> true; true -> false end end, Buff),
            case X of
                []  ->
                    Pid ! {des, error1}, descriptor(Buff,I);
                [{Y,Z,W}] ->
                    [{Z2,_}] = lists:filter(fun({_,A}) -> A == FD end, Z),
                    if
                        Pid ==   Z2 ->
                            Pid ! {des, Y, W},
                            if
                                Flag == 1 ->
                                    if
                                        length(Z) == 1 ->
                                            descriptor(Buff -- [{Y,Z,W}],I);
                                        true ->
                                            descriptor(borrarDes(Y,Z2,Buff), I)
                                    end;
                                true ->
                                    descriptor(Buff,I)
                            end;
                        true ->
                            Pid ! {des, error2}, descriptor(Buff,I)
                    end
            end
    end.

analizar_opn(L) when length(L) /= 4 -> error1;
analizar_opn([_,_,_,"\r\n"]) -> error1;
analizar_opn([_,X,M,Y]) ->
    if 
        X == "MODE" ->
            case M of
                "RDWR" ->
                    {lye,Y};
                "RDONLY" ->
                    {l,Y};
                _ ->
                    error1
            end;
        true ->
            error1
    end.

                                                %"analizar" determina los posibles errores y el nombre del archivo, si existe,
                                                %para el pedido, dado en el 1º argumento, por los operadores WRT o REA.

analizar(T, rea) when length(T) /= 5 -> error2;
analizar([_,_,_,_,"\r\n"], rea) -> error2;
analizar([_,"FD", X, "SIZE", Y], rea) -> 
    FD = fst(string:to_integer(X)),
    Size = fst(string:to_integer((Y -- "\r\n"))),
    if
        is_integer(FD) and is_integer(Size) and (FD > 0) and (Size >= 0)->
            case nd(FD,0) of
                {N,P} ->
                    {N,P,Size};
                E ->
                    E
            end;
        true ->
            error2
    end;
analizar(_,rea) -> error2;
analizar(T, wrt) when length(T) < 5 -> error2;
analizar([_,_,_,_,_,"\r\n"], wrt) -> error2;
analizar([_,"FD", X, "SIZE",Y|Z], wrt) -> 
    FD = fst(string:to_integer(X)),
    Size = fst(string:to_integer(Y)),
    Buff = string:join(Z," ") -- "\r\n",
    if
        is_integer(FD) and is_integer(Size) and (FD > 0) and (Size >= 0)->
            if
                (Size > 0) and (Buff == []) ->
                    error2;
                Size == length(Buff) ->
                    case nd(FD,0) of
                        {N,P} ->
                            {N,Size,P,Buff};
                        E ->
                            E
                    end;
                true ->
                    error1
            end;
        true ->
            error2
    end;
analizar(_,wrt) -> error2.

                                                %"nd" obtiene el nombre y pid asociado al descriptor FD.
                                                %Invocada por "analizar", y por el op. CLO.
nd(FD,Flag) ->
    desc ! {des, Flag, FD, self()},
    receive
        {des, error1}->
            error3;
        {des, error2} ->
            error4;
        {des, N, P}->
            {N,P}
    end.

salir([]) ->
    ok;
salir([{N,M,P}|L]) ->
    P ! {clo,N,M,P, self()},
    receive {clo,ok} ->
            desc ! {des,borrar,N,self()},
            receive {des, ok} -> salir(L) end
    end.

fst({X,_}) -> X.

numero(P,L) -> numero(P,L,1).

numero(P,[{P,_}|_],N) ->
    N;
numero(P,[_|L],N) ->
    numero(P,L,N+1);
numero(_,[],_) ->
    error.
