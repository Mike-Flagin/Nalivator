import {useState} from 'react';

import './App.css'
import Spinner from "./components/Spinner/Spinner.jsx";

function App() {
    const [portion, setPortion] = useState(5);

    const clickHandler = () => {
        console.log(portion);
    }

    return (
        <>
            <section id="center">
                <Spinner
                    val={portion}
                    min={1}
                    max={10}
                    step={0.5}
                    onChange={val => setPortion(val)}
                />
            </section>
            <button onClick={clickHandler}>Button</button>
        </>
    )
}

export default App
