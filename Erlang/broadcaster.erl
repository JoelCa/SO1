-module(broadcaster).
-compile(export_all).

crearBroad() ->
    P = spawn(?MODULE, servidor, [[]]),
    register(pids, P).

servidor(List) ->
    receive
        {suscribirme, Pid} ->
            X = busca(Pid,List),
            if
                X == false -> Pid ! true, servidor([Pid|List]);
                true -> Pid ! false, servidor(List)
            end;
        {desuscribirme, Pid} ->
            X = busca(Pid,List),
            if
                X == true -> Pid ! true, servidor(List--[Pid]);
                true -> Pid ! false, servidor(List)
            end;
        {msj, Pid, M} ->
            X = List -- [Pid], reenviarMsj(M,X), Pid ! {resp,recibirMsj(length(X))}, servidor(List)
    end.

suscribirse() ->
    pids ! {suscribirme, self()},
    receive
        true -> ok;
        false -> ok
    end.

desuscribirse() ->
    pids ! {desuscribirme, self()},
    receive
        true -> ok;
        false -> ok
    end.

enviarMsj(M) ->
    pids ! {msj, self(), M}, receive {resp, X} -> X end.

msjSD(M) ->
    pids ! M.  

busca(_,[]) -> false;
busca(X,[X|_]) -> true;
busca(X,[_|T]) -> busca(X,T).

reenviarMsj(_,[]) -> ok;
reenviarMsj(M,[P|T]) -> P ! M, reenviarMsj(M,T).

recibirMsj(0) -> [];
recibirMsj(N) -> receive X -> [X|recibirMsj(N-1)] end.

wait(N) ->
    receive
    after N -> ok
    end.
